/* DeviceDialog.h — Selección de la disquetera de destino/origen.
 *
 * Portado de write_image_dialog.py, conservando su filosofía de seguridad:
 *  · sólo se muestran unidades extraíbles y disqueteras (los discos internos
 *    quedan ocultos tras una casilla explícita),
 *  · se avisa si el tamaño de la imagen no encaja,
 *  · para GRABAR hay que teclear la palabra «GRABAR».
 *
 * Este diálogo sólo elige el dispositivo y confirma; la escritura la lanza
 * quien lo invoca, mediante un FloppyWorker.
 */
#ifndef DEVICEDIALOG_H
#define DEVICEDIALOG_H

#include <QDialog>
#include <QString>

class QListWidget;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

extern "C" {
#include "volumes.h"
}

class DeviceDialog : public QDialog {
    Q_OBJECT
public:
    /* mode = Write exige la confirmación «GRABAR»; mode = Read (leer del
     * disquete) o Format sólo requiere seleccionar. imageSize se usa para
     * avisar de descuadres de tamaño (0 si no aplica). */
    enum class Mode { Read, Write, Format };

    DeviceDialog(Mode mode, qint64 imageSize, QWidget *parent = nullptr);

    /* Ruta del dispositivo elegido (p. ej. "/dev/fd0"), válida tras accept(). */
    QString selectedDevice() const { return selectedDevice_; }
    /* Punto(s) de montaje del dispositivo elegido, si estaba montado. */
    bool selectedWasMounted() const { return selectedMounted_; }

private slots:
    void populate();
    void onSelectionChanged();
    void updateOkButton();
    void onAccept();

private:
    Mode mode_;
    qint64 imageSize_;
    QString selectedDevice_;
    bool selectedMounted_ = false;

    QListWidget *list_ = nullptr;
    QCheckBox   *showAll_ = nullptr;
    QLabel      *info_ = nullptr;
    QLineEdit   *confirm_ = nullptr;
    QPushButton *okButton_ = nullptr;
};

#endif // DEVICEDIALOG_H
