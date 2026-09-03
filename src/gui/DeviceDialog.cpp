/* DeviceDialog.cpp — ver DeviceDialog.h. Portado de write_image_dialog.py. */
#include "DeviceDialog.h"

#include <QCheckBox>
#include <QtGlobal>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

#include <cstdlib>

/* Guardamos el WriteTarget en el item vía una copia en el heap referenciada
 * por un QVariant que contiene el índice; para simplificar, guardamos los
 * campos que necesitamos directamente en roles de usuario. */
namespace {
constexpr int RolePath      = Qt::UserRole + 1;
constexpr int RoleSafe      = Qt::UserRole + 2;
constexpr int RoleSizeBytes = Qt::UserRole + 3;
constexpr int RoleMounted   = Qt::UserRole + 4;
constexpr int RoleSizeLabel = Qt::UserRole + 5;

QString describeTarget(const WriteTarget &t) {
    QStringList parts;
    parts << QString::fromUtf8(t.path);
    if (t.size_label[0]) parts << QString::fromUtf8(t.size_label);
    if (t.model[0])      parts << QString::fromUtf8(t.model);
    if (write_target_is_floppy(&t)) parts << QStringLiteral("disquetera");
    else if (t.removable)           parts << QStringLiteral("extraíble");
    return parts.join(QStringLiteral("  ·  "));
}
} // namespace

DeviceDialog::DeviceDialog(Mode mode, qint64 imageSize, QWidget *parent)
    : QDialog(parent), mode_(mode), imageSize_(imageSize) {

    const bool writing = (mode_ == Mode::Write);
    setWindowTitle(writing ? tr("Grabar imagen en disquete físico")
                  : mode_ == Mode::Format ? tr("Formatear disquete")
                  : tr("Leer disquete físico"));
    setMinimumWidth(620);

    auto *lay = new QVBoxLayout(this);
    lay->setSpacing(10);

    if (writing || mode_ == Mode::Format) {
        auto *warn = new QLabel(
            tr("⚠ Esta operación <b>borra por completo</b> el contenido del "
               "dispositivo elegido. Comprueba dos veces que seleccionas la "
               "unidad correcta: no hay forma de deshacerlo."));
        warn->setWordWrap(true);
        warn->setStyleSheet(QStringLiteral("color: #ff5f6d;"));
        lay->addWidget(warn);
    }

    lay->addWidget(new QLabel(tr("Dispositivo:")));
    list_ = new QListWidget;
    connect(list_, &QListWidget::currentItemChanged,
            this, &DeviceDialog::onSelectionChanged);
    lay->addWidget(list_, 1);

    auto *row = new QHBoxLayout;
    showAll_ = new QCheckBox(tr("Mostrar también discos NO extraíbles (peligroso)"));
    // La señal cambió de nombre en Qt 6.7: stateChanged(int) quedó obsoleta
    // en favor de checkStateChanged(Qt::CheckState). Elegimos según versión
    // para no arrastrar warnings y seguir compilando en Qt5 y Qt6 antiguos.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(showAll_, &QCheckBox::checkStateChanged, this, &DeviceDialog::populate);
#else
    connect(showAll_, &QCheckBox::stateChanged, this, &DeviceDialog::populate);
#endif
    row->addWidget(showAll_, 1);
    auto *refresh = new QPushButton(tr("Actualizar"));
    connect(refresh, &QPushButton::clicked, this, &DeviceDialog::populate);
    row->addWidget(refresh);
    lay->addLayout(row);

    info_ = new QLabel;
    info_->setWordWrap(true);
    info_->setStyleSheet(QStringLiteral("color: #ffb454;"));
    lay->addWidget(info_);

    if (writing) {
        auto *crow = new QHBoxLayout;
        crow->addWidget(new QLabel(tr("Para confirmar, escribe <b>GRABAR</b>:")));
        confirm_ = new QLineEdit;
        confirm_->setMaxLength(10);
        connect(confirm_, &QLineEdit::textChanged, this, &DeviceDialog::updateOkButton);
        crow->addWidget(confirm_, 1);
        lay->addLayout(crow);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    okButton_ = new QPushButton(writing ? tr("Grabar")
                                : mode_ == Mode::Format ? tr("Formatear")
                                : tr("Leer"));
    okButton_->setEnabled(false);
    connect(okButton_, &QPushButton::clicked, this, &DeviceDialog::onAccept);
    buttons->addButton(okButton_, QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(buttons);

    populate();
}

void DeviceDialog::populate() {
    list_->clear();
    const bool showAll = showAll_->isChecked();

    WriteTarget *targets = nullptr;
    size_t n = 0;
    list_write_targets(&targets, &n);

    bool any = false;
    for (size_t i = 0; i < n; i++) {
        const WriteTarget &t = targets[i];
        bool safe = write_target_looks_safe(&t);
        if (!safe && !showAll)
            continue;

        QString text = describeTarget(t);
        if (t.mountpoint[0])
            text += tr("   [montado en %1]").arg(QString::fromUtf8(t.mountpoint));

        auto *item = new QListWidgetItem(text);
        item->setData(RolePath,      QString::fromUtf8(t.path));
        item->setData(RoleSafe,      safe);
        item->setData(RoleSizeBytes, static_cast<qulonglong>(t.size_bytes));
        item->setData(RoleMounted,   t.mountpoint[0] != '\0');
        item->setData(RoleSizeLabel, QString::fromUtf8(t.size_label));
        if (!safe) {
            item->setForeground(Qt::red);
            item->setText(tr("⚠ NO EXTRAÍBLE — ") + text);
        }
        list_->addItem(item);
        any = true;
    }
    free(targets);

    if (!any) {
        auto *item = new QListWidgetItem(
            tr("(no se detectó ninguna unidad extraíble ni disquetera)"));
        item->setFlags(Qt::NoItemFlags);
        list_->addItem(item);
    }
    updateOkButton();
}

void DeviceDialog::onSelectionChanged() {
    auto *item = list_->currentItem();
    QStringList notes;
    if (item && (item->flags() & Qt::ItemIsSelectable)) {
        bool safe = item->data(RoleSafe).toBool();
        qulonglong size = item->data(RoleSizeBytes).toULongLong();
        QString sizeLabel = item->data(RoleSizeLabel).toString();

        if (!safe)
            notes << tr("Este dispositivo NO parece extraíble. Si es un disco "
                        "del sistema, escribir aquí destruiría su contenido.");
        if (size && imageSize_ > 0) {
            if (static_cast<qint64>(size) < imageSize_)
                notes << tr("La imagen (%1 KB) es MÁS GRANDE que el dispositivo "
                            "(%2): no cabe.")
                            .arg(imageSize_ / 1024).arg(sizeLabel);
            else if (static_cast<qint64>(size) > imageSize_ * 4)
                notes << tr("El dispositivo (%1) es mucho mayor que la imagen "
                            "(%2 KB). ¿Seguro que no es una memoria USB de datos?")
                            .arg(sizeLabel).arg(imageSize_ / 1024);
        }
        if (item->data(RoleMounted).toBool())
            notes << tr("Está montado; se desmontará automáticamente antes de operar.");
    }
    info_->setText(notes.join(QStringLiteral("  ")));
    info_->setVisible(!notes.isEmpty());
    updateOkButton();
}

void DeviceDialog::updateOkButton() {
    auto *item = list_->currentItem();
    bool hasSel = item && (item->flags() & Qt::ItemIsSelectable);

    bool confirmed = true;
    if (mode_ == Mode::Write)
        confirmed = confirm_ && confirm_->text().trimmed().toUpper()
                    == QStringLiteral("GRABAR");

    bool fits = true;
    if (hasSel && imageSize_ > 0) {
        qulonglong size = item->data(RoleSizeBytes).toULongLong();
        if (size) fits = static_cast<qint64>(size) >= imageSize_;
    }
    okButton_->setEnabled(hasSel && confirmed && fits);
}

void DeviceDialog::onAccept() {
    auto *item = list_->currentItem();
    if (!item)
        return;
    selectedDevice_ = item->data(RolePath).toString();
    selectedMounted_ = item->data(RoleMounted).toBool();
    accept();
}
