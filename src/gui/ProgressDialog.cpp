/* ProgressDialog.cpp — ver ProgressDialog.h. */
#include "ProgressDialog.h"
#include "FloppyWorker.h"
#include "TrackGrid.h"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

/* Estados de pista (coinciden con TrackStatus del núcleo). */
enum { ST_OK = 1, ST_RETRY = 2, ST_BAD = 3, ST_CUR = 4 };

/* Crea una entrada de leyenda: un cuadradito de color y su texto. */
static QWidget *makeLegend(const QString &color, const QString &text,
                           bool borderWhite = false) {
    auto *w = new QWidget;
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
    auto *sw = new QLabel;
    sw->setFixedSize(12, 12);
    sw->setStyleSheet(QStringLiteral(
        "background:%1; border-radius:2px;%2")
        .arg(color, borderWhite ? QStringLiteral(" border:1px solid white;") : QString()));
    l->addWidget(sw);
    auto *t = new QLabel(text);
    t->setStyleSheet(QStringLiteral("color:#c9d9ec; font-size:12px;"));
    l->addWidget(t);
    return w;
}

ProgressDialog::ProgressDialog(FloppyWorker *worker, const QString &title,
                               int tracks, QWidget *parent)
    : QDialog(parent), worker_(worker) {
    setWindowTitle(title);
    setMinimumWidth(520);
    setModal(true);
    // Fondo azul oscuro, como la cabecera, para el aire "estación de copia".
    setStyleSheet(QStringLiteral("QDialog { background: #20405e; }"));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(18, 18, 18, 18);
    lay->setSpacing(12);

    // Cabecera: operación + posición de pista.
    auto *head = new QHBoxLayout;
    label_ = new QLabel(tr("Preparando…"));
    label_->setStyleSheet(QStringLiteral("color:white; font-size:14px; font-weight:500;"));
    head->addWidget(label_);
    head->addStretch(1);
    posLabel_ = new QLabel;
    posLabel_->setStyleSheet(QStringLiteral("color:#c9d9ec; font-size:13px;"));
    head->addWidget(posLabel_);
    lay->addLayout(head);

    // Rejilla de pistas.
    grid_ = new TrackGrid;
    grid_->reset(tracks > 0 ? tracks : 80);
    lay->addWidget(grid_);

    // Barra de progreso, integrada debajo.
    bar_ = new QProgressBar;
    bar_->setRange(0, 100);
    bar_->setTextVisible(false);
    bar_->setFixedHeight(14);
    bar_->setStyleSheet(QStringLiteral(
        "QProgressBar { background:#15314a; border:1px solid #42648c;"
        "  border-radius:7px; }"
        "QProgressBar::chunk { background:#4aa3e0; border-radius:6px; }"));
    lay->addWidget(bar_);

    // Leyenda de colores.
    auto *legend = new QHBoxLayout;
    legend->setSpacing(16);
    legend->addWidget(makeLegend(QStringLiteral("#2e9e6f"), tr("Correcta")));
    legend->addWidget(makeLegend(QStringLiteral("#e89a2c"), tr("Reintentos")));
    legend->addWidget(makeLegend(QStringLiteral("#d84a47"), tr("Error")));
    legend->addWidget(makeLegend(QStringLiteral("#f2b705"), tr("Actual"), true));
    legend->addStretch(1);
    lay->addLayout(legend);

    // Botón cancelar.
    auto *cancel = new QPushButton(tr("Cancelar"));
    cancel->setStyleSheet(QStringLiteral(
        "QPushButton { background:white; color:#20405e; border:none;"
        "  border-radius:6px; padding:7px 16px; font-weight:bold; }"));
    connect(cancel, &QPushButton::clicked, this, &ProgressDialog::onCancel);
    auto *brow = new QHBoxLayout;
    brow->addStretch(1);
    brow->addWidget(cancel);
    lay->addLayout(brow);

    connect(worker_, &FloppyWorker::progress, this, &ProgressDialog::onProgress);
    connect(worker_, &FloppyWorker::finishedOk, this, &ProgressDialog::onFinished);

    worker_->start();
}

void ProgressDialog::onProgress(qint64 done, qint64 total, int trackIndex,
                                int cylinder, int head, int status) {
    if (total > 0) {
        int pct = static_cast<int>((done * 100) / total);
        bar_->setValue(pct);
    }
    // Cada aviso llega cuando una pista YA se ha procesado, con su resultado.
    // Pintamos esa pista con su color definitivo (verde/naranja/rojo) y, para
    // dar sensación de avance en vivo, marcamos en amarillo la SIGUIENTE, que
    // es la que la disquetera está procesando en este instante.
    if (grid_) {
        grid_->setTrack(trackIndex, status);          // estado real
        grid_->setTrack(trackIndex + 1, ST_CUR);      // la siguiente = actual
        lastTrack_ = trackIndex;
    }
    label_->setText(tr("Procesando pista %1").arg(cylinder));
    posLabel_->setText(tr("Pista %1 · Cara %2").arg(cylinder).arg(head));
}

void ProgressDialog::onFinished(bool ok, const QString &message) {
    ok_ = ok;
    message_ = message;
    worker_->wait();
    if (ok)
        accept();
    else
        reject();
}

void ProgressDialog::onCancel() {
    if (cancelling_)
        return;
    cancelling_ = true;
    label_->setText(tr("Cancelando…"));
    worker_->requestCancel();
}
