/* MainWindow.h — Ventana principal de COPIA720.
 *
 * Cuatro acciones, como pediste:
 *   · Crear imagen desde disquete real   (leer /dev/fdX → archivo .img)
 *   · Copiar imagen a disquete real       (archivo .img → /dev/fdX)
 *   · Explorar disquete                   (parsear FAT12 y navegar carpetas)
 *   · Formatear                           (formateo de bajo nivel pista a pista)
 *
 * La lógica pesada vive en el núcleo C (../core). Esta clase sólo orquesta
 * diálogos e hilos de trabajo para no bloquear la interfaz.
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

extern "C" {
#include "floppy_device.h"
}
#include "FloppyWorker.h"   // FloppyBackend

class QLabel;
class QComboBox;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onCreateImageFromFloppy();   // disquete → imagen
    void onWriteImageToFloppy();      // imagen → disquete
    void onExploreFloppy();           // explorar FAT12
    void onFormatFloppy();            // formatear

    void onBackendChanged();          // cambia FDC / Greaseweazle
    void showToolsCheck();            // Ayuda → Comprobar herramientas
    void showHowTo();                 // Ayuda → Cómo empezar
    void showAbout();                 // Ayuda → Acerca de

private:
    /* Backend y geometría elegidos ahora mismo en los selectores. */
    FloppyBackend currentBackend() const;
    const FloppyGeometry &currentGeom() const;

    /* Construye la cabecera azul con el título y las pastillas de selección. */
    QWidget *buildHeader();
    /* Refresca el aspecto "on/off" de las pastillas según los combos ocultos. */
    void refreshPills();

    QComboBox *backendCombo_ = nullptr;   // lógica (oculto)
    QComboBox *formatCombo_ = nullptr;     // lógica (oculto)

    // Pastillas visibles de la cabecera.
    class QPushButton *pillFdc_ = nullptr;
    class QPushButton *pillGw_ = nullptr;
    class QPushButton *pill720_ = nullptr;
    class QPushButton *pill1440_ = nullptr;

    QLabel *statusHint_ = nullptr;
};

#endif // MAINWINDOW_H
