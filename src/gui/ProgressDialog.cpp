/* ProgressDialog.cpp — ver ProgressDialog.h. */
#include "ProgressDialog.h"
#include "FloppyWorker.h"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ProgressDialog::ProgressDialog(FloppyWorker *worker, const QString &title,
                               QWidget *parent)
    : QDialog(parent), worker_(worker) {
    setWindowTitle(title);
    setMinimumWidth(420);
    setModal(true);

    auto *lay = new QVBoxLayout(this);
    label_ = new QLabel(tr("Preparando…"));
    lay->addWidget(label_);

    bar_ = new QProgressBar;
    bar_->setRange(0, 100);
    lay->addWidget(bar_);

    auto *cancel = new QPushButton(tr("Cancelar"));
    connect(cancel, &QPushButton::clicked, this, &ProgressDialog::onCancel);
    lay->addWidget(cancel);

    connect(worker_, &FloppyWorker::progress, this, &ProgressDialog::onProgress);
    connect(worker_, &FloppyWorker::finishedOk, this, &ProgressDialog::onFinished);

    worker_->start();
}

void ProgressDialog::onProgress(qint64 done, qint64 total) {
    if (total > 0) {
        int pct = static_cast<int>((done * 100) / total);
        bar_->setValue(pct);
        label_->setText(tr("Procesando… %1 KB de %2 KB")
                        .arg(done / 1024).arg(total / 1024));
    }
}

void ProgressDialog::onFinished(bool ok, const QString &message) {
    ok_ = ok;
    message_ = message;
    worker_->wait();
    // Nota: no destruimos el worker aquí. El llamante puede necesitar leer
    // su resultado (p. ej. la imagen recién leída) después de exec(). La
    // propiedad del worker es de quien construye el ProgressDialog.
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
