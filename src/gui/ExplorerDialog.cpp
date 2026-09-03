/* ExplorerDialog.cpp — ver ExplorerDialog.h. */
#include "ExplorerDialog.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cstdlib>
#include <cstdio>

namespace {
/* Roles para asociar cada item del árbol con su entrada FAT12. */
constexpr int RoleEntryPtr = Qt::UserRole + 1;   // const Fat12Entry*
constexpr int RoleIsDir    = Qt::UserRole + 2;

QString humanSize(uint32_t n) {
    if (n < 1024) return QString::number(n) + " B";
    if (n < 1024 * 1024) return QString::number(n / 1024.0, 'f', 1) + " KB";
    return QString::number(n / 1048576.0, 'f', 2) + " MB";
}
} // namespace

ExplorerDialog::ExplorerDialog(std::vector<uint8_t> image, QString title,
                               QWidget *parent)
    : QDialog(parent), image_(std::move(image)) {
    setWindowTitle(tr("Explorar — %1").arg(title));
    setMinimumSize(560, 460);

    auto *lay = new QVBoxLayout(this);

    int rc = fat12_parse(image_.data(), image_.size(), &fat_);
    parsedOk_ = (rc == FAT12_OK);

    if (!parsedOk_) {
        auto *err = new QLabel(
            tr("No se pudo interpretar el contenido como un disco FAT12.\n"
               "¿Seguro que es una imagen de disquete MSX-DOS / DOS válida?"));
        err->setWordWrap(true);
        lay->addWidget(err);
        auto *close = new QPushButton(tr("Cerrar"));
        connect(close, &QPushButton::clicked, this, &QDialog::reject);
        lay->addWidget(close);
        return;
    }

    tree_ = new QTreeWidget;
    tree_->setColumnCount(3);
    tree_->setHeaderLabels({tr("Nombre"), tr("Tamaño"), tr("Atributos")});
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(tree_, &QTreeWidget::itemSelectionChanged,
            this, &ExplorerDialog::onSelectionChanged);
    lay->addWidget(tree_, 1);

    populateTree(nullptr, fat_.entries, fat_.entry_count);
    tree_->expandToDepth(0);

    size_t nf = 0, nd = 0;
    fat12_count(fat_.entries, fat_.entry_count, &nf, &nd);
    summary_ = new QLabel(tr("%1 archivo(s), %2 carpeta(s)  ·  %3 por sector, "
                             "%4 sectores")
                          .arg(nf).arg(nd).arg(fat_.bps).arg(fat_.total_sectors));
    lay->addWidget(summary_);

    auto *row = new QHBoxLayout;
    row->addStretch(1);
    auto *extract = new QPushButton(tr("Extraer archivo…"));
    connect(extract, &QPushButton::clicked, this, &ExplorerDialog::onExtractSelected);
    row->addWidget(extract);
    auto *close = new QPushButton(tr("Cerrar"));
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    row->addWidget(close);
    lay->addLayout(row);
}

ExplorerDialog::~ExplorerDialog() {
    if (parsedOk_)
        fat12_free(&fat_);
}

void ExplorerDialog::populateTree(QTreeWidgetItem *parent,
                                  const Fat12Entry *entries, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const Fat12Entry *e = &entries[i];
        QStringList cols;
        cols << QString::fromUtf8(e->name);
        cols << (e->is_dir ? QString() : humanSize(e->size));
        QStringList attrs;
        if (e->attr & 0x01) attrs << QStringLiteral("R");
        if (e->attr & 0x02) attrs << QStringLiteral("H");
        if (e->attr & 0x04) attrs << QStringLiteral("S");
        if (e->attr & 0x10) attrs << QStringLiteral("DIR");
        if (e->attr & 0x20) attrs << QStringLiteral("A");
        cols << attrs.join(QStringLiteral(" "));

        auto *item = parent ? new QTreeWidgetItem(parent, cols)
                            : new QTreeWidgetItem(tree_, cols);
        item->setData(0, RoleEntryPtr,
                      QVariant::fromValue(reinterpret_cast<quintptr>(e)));
        item->setData(0, RoleIsDir, e->is_dir);

        if (e->is_dir)
            populateTree(item, e->children, e->child_count);
    }
}

void ExplorerDialog::onSelectionChanged() {
    /* Nada extra por ahora; el botón extraer valida por su cuenta. */
}

void ExplorerDialog::onExtractSelected() {
    auto items = tree_->selectedItems();
    if (items.isEmpty()) {
        QMessageBox::information(this, tr("Extraer"),
                                 tr("Selecciona primero un archivo."));
        return;
    }
    auto *item = items.first();
    if (item->data(0, RoleIsDir).toBool()) {
        QMessageBox::information(this, tr("Extraer"),
                                 tr("Selecciona un archivo, no una carpeta."));
        return;
    }
    auto ptr = item->data(0, RoleEntryPtr).value<quintptr>();
    const Fat12Entry *e = reinterpret_cast<const Fat12Entry *>(ptr);

    QString suggested = QString::fromUtf8(e->name);
    QString dest = QFileDialog::getSaveFileName(
        this, tr("Guardar archivo extraído"), suggested);
    if (dest.isEmpty())
        return;

    size_t len = 0;
    uint8_t *data = fat12_read_file(&fat_, e, &len);
    if (!data) {
        QMessageBox::warning(this, tr("Extraer"),
                             tr("No se pudo leer el archivo del disco."));
        return;
    }

    FILE *f = fopen(dest.toUtf8().constData(), "wb");
    bool ok = f && (len == 0 || fwrite(data, 1, len, f) == len);
    if (f) fclose(f);
    free(data);

    if (ok)
        QMessageBox::information(this, tr("Extraer"),
                                 tr("Archivo extraído correctamente (%1).")
                                 .arg(humanSize(static_cast<uint32_t>(len))));
    else
        QMessageBox::warning(this, tr("Extraer"),
                             tr("No se pudo escribir el archivo de destino."));
}
