/* ProgressDialog.h — Diálogo modal de progreso para una operación de disco.
 *
 * Envuelve un FloppyWorker: muestra una barra de progreso por pistas, permite
 * cancelar, y al terminar informa del resultado. Devuelve QDialog::Accepted
 * si la operación terminó con éxito.
 */
#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QString>

class QProgressBar;
class QLabel;
class FloppyWorker;
class TrackGrid;

class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    /* Arranca el worker y muestra su progreso. NO toma posesión: el llamante
     * es responsable de destruirlo (con deleteLater) tras exec(), momento en
     * que su resultado ya se ha podido leer. `tracks` es el número de pistas
     * del formato, para dimensionar la rejilla. */
    ProgressDialog(FloppyWorker *worker, const QString &title,
                   int tracks, QWidget *parent = nullptr);

    QString resultMessage() const { return message_; }
    bool succeeded() const { return ok_; }

private slots:
    void onProgress(qint64 done, qint64 total, int trackIndex,
                    int cylinder, int head, int status);
    void onFinished(bool ok, const QString &message);
    void onCancel();

private:
    FloppyWorker *worker_;
    TrackGrid *grid_ = nullptr;
    QProgressBar *bar_ = nullptr;
    QLabel *label_ = nullptr;
    QLabel *posLabel_ = nullptr;
    int lastTrack_ = -1;
    QString message_;
    bool ok_ = false;
    bool cancelling_ = false;
};

#endif // PROGRESSDIALOG_H
