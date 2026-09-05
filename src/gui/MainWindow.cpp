/* MainWindow.cpp — ver MainWindow.h. */
#include "MainWindow.h"

#include "DeviceDialog.h"
#include "DiskButton.h"
#include "ExplorerDialog.h"
#include "FloppyWorker.h"
#include "ProgressDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <vector>
#include <cstdint>
#include <cstdio>

extern "C" {
#include "disk_image.h"
#include "volumes.h"
#include "floppy_format.h"
#include "floppy_greaseweazle.h"
}

// ---------------------------------------------------------------------------
// Backend y geometría seleccionados
// ---------------------------------------------------------------------------

FloppyBackend MainWindow::currentBackend() const {
    if (backendCombo_ && backendCombo_->currentData().toInt() == 1)
        return FloppyBackend::Greaseweazle;
    return FloppyBackend::Fdc;
}

const FloppyGeometry &MainWindow::currentGeom() const {
    if (formatCombo_ && formatCombo_->currentData().toInt() == 1440)
        return FLOPPY_1440;
    return FLOPPY_720;
}

// ---------------------------------------------------------------------------
// Construcción de la ventana
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("COPIA720 — disquetes de 3½"));

    // --- menú Ayuda ---
    auto *help = menuBar()->addMenu(tr("A&yuda"));
    help->addAction(tr("Comprobar herramientas…"), this, &MainWindow::showToolsCheck);
    help->addAction(tr("Cómo empezar…"), this, &MainWindow::showHowTo);
    help->addSeparator();
    help->addAction(tr("Acerca de COPIA720"), this, &MainWindow::showAbout);

    auto *central = new QWidget;
    auto *lay = new QVBoxLayout(central);
    lay->setSpacing(14);
    lay->setContentsMargins(24, 24, 24, 24);

    auto *title = new QLabel(
        QStringLiteral("<h2>COPIA720</h2>"
                       "<p>Guardar y restaurar disquetes de 3½, y explorar su "
                       "contenido. Compatible con disquetera clásica (FDC) y "
                       "con Greaseweazle.</p>"));
    title->setWordWrap(true);
    lay->addWidget(title);

    // --- selectores de backend y formato ---
    auto *selBox = new QGroupBox(tr("Disquetera y formato"));
    auto *form = new QFormLayout(selBox);

    backendCombo_ = new QComboBox;
    backendCombo_->addItem(tr("Disquetera clásica (controlador FDC, /dev/fd0)"), 0);
    backendCombo_->addItem(tr("Greaseweazle (USB)"), 1);
    connect(backendCombo_,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onBackendChanged);
    form->addRow(tr("Usar:"), backendCombo_);

    formatCombo_ = new QComboBox;
    formatCombo_->addItem(tr("720 KB (doble densidad)"), 720);
    formatCombo_->addItem(tr("1.44 MB (alta densidad)"), 1440);
    form->addRow(tr("Formato:"), formatCombo_);

    lay->addWidget(selBox);

    // --- botones de acción (disquetes) ---
    auto *btnGrid = new QGridLayout;
    btnGrid->setSpacing(16);

    auto makeButton = [&](const QString &basename, const QString &tip,
                          const QString &desc, void (MainWindow::*slot)(),
                          int row, int col) {
        (void)desc;   // la descripción va ahora en el tooltip (tip), no debajo
        auto *btn = new DiskButton(basename, tip);
        btn->setFixedSize(150, 150);
        connect(btn, &QAbstractButton::clicked, this, slot);
        btnGrid->addWidget(btn, row, col, Qt::AlignHCenter);
    };

    btnGrid->setVerticalSpacing(20);

    makeButton(QStringLiteral("crear"),
               tr("Crear imagen desde disquete real: lee un disquete físico "
                  "completo y lo guarda en un archivo .img."),
               QString(), &MainWindow::onCreateImageFromFloppy, 0, 0);

    makeButton(QStringLiteral("copiar"),
               tr("Copiar imagen a disquete real: graba un archivo .img sobre "
                  "un disquete. Opción de formatear al vuelo y verificar."),
               QString(), &MainWindow::onWriteImageToFloppy, 0, 1);

    makeButton(QStringLiteral("explorar"),
               tr("Explorar disquete: abre el contenido FAT12 de una imagen o "
                  "de un disquete físico y permite extraer archivos."),
               QString(), &MainWindow::onExploreFloppy, 1, 0);

    makeButton(QStringLiteral("formatear"),
               tr("Formatear disquete: formateo de bajo nivel, pista a pista. "
                  "Sólo con la disquetera clásica (FDC)."),
               QString(), &MainWindow::onFormatFloppy, 1, 1);

    lay->addLayout(btnGrid);

    statusHint_ = new QLabel;
    statusHint_->setWordWrap(true);
    statusHint_->setStyleSheet(QStringLiteral("color: #ffb454;"));
    lay->addWidget(statusHint_);

    lay->addStretch(1);
    setCentralWidget(central);
    setMinimumWidth(520);

    onBackendChanged();   // ajustar avisos iniciales
}

// ---------------------------------------------------------------------------
// Cambio de backend: ajustar avisos y disponibilidad
// ---------------------------------------------------------------------------

void MainWindow::onBackendChanged() {
    QString hint;
    if (currentBackend() == FloppyBackend::Greaseweazle) {
        if (!gw_available()) {
            hint = tr("⚠ No se encontró la herramienta «gw» de Greaseweazle. "
                      "Ábre Ayuda → Comprobar herramientas para ver cómo "
                      "instalarla.");
        } else {
            hint = tr("Greaseweazle seleccionada. El formateo de bajo nivel no "
                      "aplica a este modo (la Greaseweazle escribe imágenes).");
        }
    } else {
        if (!floppy_format_supported())
            hint = tr("Nota: el formateo de bajo nivel sólo está disponible en "
                      "Linux con controlador de disquete (FDC).");
    }
    statusHint_->setText(hint);
    statusHint_->setVisible(!hint.isEmpty());
}

// ---------------------------------------------------------------------------
// disquete → imagen
// ---------------------------------------------------------------------------

void MainWindow::onCreateImageFromFloppy() {
    const FloppyBackend backend = currentBackend();
    const FloppyGeometry &geom = currentGeom();
    QString device;

    if (backend == FloppyBackend::Greaseweazle) {
        if (!gw_available()) {
            QMessageBox::warning(this, tr("Greaseweazle"),
                tr("No se encontró «gw». Instálala desde Ayuda → Comprobar "
                   "herramientas."));
            return;
        }
    } else {
        DeviceDialog dev(DeviceDialog::Mode::Read, 0, this);
        if (dev.exec() != QDialog::Accepted)
            return;
        device = dev.selectedDevice();
        if (dev.selectedWasMounted()) {
            char err[256];
            volume_unmount(device.toUtf8().constData(), err, sizeof err);
        }
    }

    QString dest = QFileDialog::getSaveFileName(
        this, tr("Guardar imagen como"), QStringLiteral("disquete.img"),
        tr("Imágenes de disco (*.img *.dsk);;Todos los archivos (*)"));
    if (dest.isEmpty())
        return;

    auto *worker = new FloppyWorker(FloppyOp::ReadImage, device, geom,
                                    {}, false, backend);
    ProgressDialog dlg(worker, tr("Leyendo disquete"), this);
    dlg.exec();

    if (!dlg.succeeded()) {
        QMessageBox::warning(this, tr("Leer disquete"),
                             tr("No se completó: %1").arg(dlg.resultMessage()));
        worker->deleteLater();
        return;
    }
    std::vector<uint8_t> captured = worker->result();
    worker->deleteLater();
    int st = image_save(dest.toUtf8().constData(), captured.data(),
                        captured.size());
    if (st != IMG_OK)
        QMessageBox::warning(this, tr("Guardar imagen"),
                             QString::fromUtf8(image_strerror(st)));
    else
        QMessageBox::information(this, tr("Guardar imagen"),
                                 tr("Imagen guardada correctamente."));
}

// ---------------------------------------------------------------------------
// imagen → disquete
// ---------------------------------------------------------------------------

void MainWindow::onWriteImageToFloppy() {
    const FloppyBackend backend = currentBackend();
    const FloppyGeometry &geom = currentGeom();

    if (backend == FloppyBackend::Greaseweazle && !gw_available()) {
        QMessageBox::warning(this, tr("Greaseweazle"),
            tr("No se encontró «gw». Instálala desde Ayuda → Comprobar "
               "herramientas."));
        return;
    }

    QString src = QFileDialog::getOpenFileName(
        this, tr("Elegir imagen a grabar"), QString(),
        tr("Imágenes de disco (*.img *.dsk);;Todos los archivos (*)"));
    if (src.isEmpty())
        return;

    size_t len = 0; int st = 0;
    uint8_t *raw = image_load(src.toUtf8().constData(), &len, &st);
    if (!raw) {
        QMessageBox::warning(this, tr("Grabar imagen"),
                             QString::fromUtf8(image_strerror(st)));
        return;
    }
    std::vector<uint8_t> data(raw, raw + len);
    free(raw);

    size_t expected = floppy_total_bytes(&geom);
    if (data.size() != expected) {
        auto r = QMessageBox::question(
            this, tr("Tamaño inesperado"),
            tr("La imagen mide %1 KB, pero el formato elegido son %2 KB. "
               "¿Grabar de todos modos?")
                .arg(data.size() / 1024).arg(expected / 1024),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (r != QMessageBox::Yes)
            return;
        data.resize(expected, 0xF6);
    }

    // Formateo al vuelo y verificación: sólo tienen sentido con la FDC.
    bool doFormat = false, doVerify = false;
    if (backend == FloppyBackend::Fdc) {
        QMessageBox opts(this);
        opts.setWindowTitle(tr("Opciones de grabación"));
        opts.setText(tr("¿Formatear cada pista antes de grabarla?\n\n"
                        "Necesario para disquetes vírgenes; más lento pero más "
                        "fiable con soportes en mal estado."));
        auto *bFormat = opts.addButton(tr("Formatear y grabar"), QMessageBox::AcceptRole);
        opts.addButton(tr("Sólo grabar"), QMessageBox::AcceptRole);
        opts.addButton(QMessageBox::Cancel);
        opts.exec();
        if (opts.clickedButton() == opts.button(QMessageBox::Cancel))
            return;
        doFormat = (opts.clickedButton() == bFormat);

        auto verify = QMessageBox::question(
            this, tr("Verificar"),
            tr("¿Verificar la grabación releyendo el disquete al terminar?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        doVerify = (verify == QMessageBox::Yes);
    }

    QString device;
    if (backend == FloppyBackend::Fdc) {
        DeviceDialog dev(DeviceDialog::Mode::Write,
                         static_cast<qint64>(data.size()), this);
        if (dev.exec() != QDialog::Accepted)
            return;
        device = dev.selectedDevice();
        if (dev.selectedWasMounted()) {
            char err[256];
            if (!volume_unmount(device.toUtf8().constData(), err, sizeof err)) {
                QMessageBox::warning(this, tr("Grabar imagen"),
                                     tr("No se pudo desmontar el dispositivo:\n%1")
                                     .arg(QString::fromUtf8(err)));
                return;
            }
        }
    } else {
        // Greaseweazle: confirmación simple (no hay selección de /dev/*).
        auto r = QMessageBox::warning(
            this, tr("Grabar con Greaseweazle"),
            tr("Se va a grabar la imagen en el disquete insertado en la "
               "Greaseweazle. Se sobrescribirá su contenido. ¿Continuar?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (r != QMessageBox::Yes)
            return;
    }

    FloppyOp op = doFormat ? FloppyOp::WriteAndFormat : FloppyOp::WriteImage;
    auto *worker = new FloppyWorker(op, device, geom, std::move(data),
                                    doVerify, backend);
    ProgressDialog dlg(worker,
                       doFormat ? tr("Formateando y grabando")
                                : tr("Grabando disquete"), this);
    dlg.exec();
    worker->deleteLater();

    if (dlg.succeeded())
        QMessageBox::information(this, tr("Grabar imagen"),
                                 tr("Grabación completada.\nExpulsa el disquete "
                                    "antes de retirarlo."));
    else
        QMessageBox::warning(this, tr("Grabar imagen"),
                             tr("No se completó: %1").arg(dlg.resultMessage()));
}

// ---------------------------------------------------------------------------
// explorar
// ---------------------------------------------------------------------------

void MainWindow::onExploreFloppy() {
    const FloppyBackend backend = currentBackend();
    const FloppyGeometry &geom = currentGeom();

    QMessageBox choose(this);
    choose.setWindowTitle(tr("Explorar disquete"));
    choose.setText(tr("¿Qué quieres explorar?"));
    auto *bFile   = choose.addButton(tr("Un archivo .img"), QMessageBox::AcceptRole);
    auto *bFloppy = choose.addButton(tr("El disquete físico"), QMessageBox::AcceptRole);
    choose.addButton(QMessageBox::Cancel);
    choose.exec();
    if (choose.clickedButton() == choose.button(QMessageBox::Cancel))
        return;

    std::vector<uint8_t> data;
    QString title;

    if (choose.clickedButton() == bFile) {
        QString src = QFileDialog::getOpenFileName(
            this, tr("Elegir imagen"), QString(),
            tr("Imágenes de disco (*.img *.dsk);;Todos los archivos (*)"));
        if (src.isEmpty())
            return;
        size_t len = 0; int st = 0;
        uint8_t *raw = image_load(src.toUtf8().constData(), &len, &st);
        if (!raw) {
            QMessageBox::warning(this, tr("Explorar"),
                                 QString::fromUtf8(image_strerror(st)));
            return;
        }
        data.assign(raw, raw + len);
        free(raw);
        title = QFileInfo(src).fileName();
    } else if (choose.clickedButton() == bFloppy) {
        QString device;
        if (backend == FloppyBackend::Greaseweazle) {
            if (!gw_available()) {
                QMessageBox::warning(this, tr("Greaseweazle"),
                    tr("No se encontró «gw». Instálala desde Ayuda → "
                       "Comprobar herramientas."));
                return;
            }
        } else {
            DeviceDialog dev(DeviceDialog::Mode::Read, 0, this);
            if (dev.exec() != QDialog::Accepted)
                return;
            device = dev.selectedDevice();
            if (dev.selectedWasMounted()) {
                char err[256];
                volume_unmount(device.toUtf8().constData(), err, sizeof err);
            }
        }
        auto *worker = new FloppyWorker(FloppyOp::ReadImage, device, geom,
                                        {}, false, backend);
        ProgressDialog dlg(worker, tr("Leyendo disquete"), this);
        dlg.exec();
        if (!dlg.succeeded()) {
            QMessageBox::warning(this, tr("Explorar"),
                                 tr("No se pudo leer: %1").arg(dlg.resultMessage()));
            worker->deleteLater();
            return;
        }
        data = worker->result();
        worker->deleteLater();
        title = tr("disquete físico");
    } else {
        return;
    }

    ExplorerDialog explorer(std::move(data), title, this);
    explorer.exec();
}

// ---------------------------------------------------------------------------
// formatear
// ---------------------------------------------------------------------------

void MainWindow::onFormatFloppy() {
    if (currentBackend() == FloppyBackend::Greaseweazle) {
        QMessageBox::information(this, tr("Formatear"),
            tr("El formateo de bajo nivel no aplica a la Greaseweazle: "
               "para dejar un disquete vacío, graba una imagen ya formateada. "
               "El formateo pista a pista sólo está disponible con la "
               "disquetera clásica (FDC)."));
        return;
    }
    if (!floppy_format_supported()) {
        QMessageBox::information(this, tr("Formatear"),
            tr("El formateo de bajo nivel sólo está disponible en Linux."));
        return;
    }
    const FloppyGeometry &geom = currentGeom();

    DeviceDialog dev(DeviceDialog::Mode::Format, 0, this);
    if (dev.exec() != QDialog::Accepted)
        return;
    QString device = dev.selectedDevice();
    if (dev.selectedWasMounted()) {
        char err[256];
        volume_unmount(device.toUtf8().constData(), err, sizeof err);
    }

    auto r = QMessageBox::warning(
        this, tr("Confirmar formateo"),
        tr("Se va a formatear a bajo nivel:\n\n    %1\n\n"
           "Esto borra todo su contenido. ¿Continuar?").arg(device),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (r != QMessageBox::Yes)
        return;

    auto *worker = new FloppyWorker(FloppyOp::FormatDisk, device, geom);
    ProgressDialog dlg(worker, tr("Formateando disquete"), this);
    dlg.exec();
    worker->deleteLater();

    if (dlg.succeeded())
        QMessageBox::information(this, tr("Formatear"),
                                 tr("Formateo completado."));
    else
        QMessageBox::warning(this, tr("Formatear"),
                             tr("No se completó: %1").arg(dlg.resultMessage()));
}

// ---------------------------------------------------------------------------
// Ayuda → Comprobar herramientas (diagnóstico)
// ---------------------------------------------------------------------------

void MainWindow::showToolsCheck() {
    auto yes = QStringLiteral("<span style='color:#4caf50'>✓</span>");
    auto no  = QStringLiteral("<span style='color:#f44336'>✗</span>");

    QString html = QStringLiteral("<h3>Estado de las herramientas</h3><ul>");

    // Greaseweazle
    if (gw_available()) {
        char ver[128] = {0};
        gw_version(ver, sizeof ver);
        html += tr("<li>%1 Greaseweazle («gw») instalada%2</li>")
                    .arg(yes)
                    .arg(ver[0] ? QStringLiteral(" — versión %1")
                                      .arg(QString::fromUtf8(ver))
                                : QString());
    } else {
        html += tr("<li>%1 Greaseweazle («gw») no encontrada.<br>"
                   "Instálala con:<br>"
                   "<code>pipx install git+https://github.com/keirf/"
                   "greaseweazle@latest</code></li>").arg(no);
    }

    // lsblk / udisksctl
    html += tr("<li>%1 Herramientas del sistema (montaje de USB): %2</li>")
                .arg(volumes_can_mount() ? yes : no)
                .arg(volumes_can_mount()
                     ? tr("disponibles")
                     : tr("falta «udisksctl» (paquete udisks2)"));

    // Formateo FDC
    html += tr("<li>%1 Formateo de bajo nivel (FDC): %2</li>")
                .arg(floppy_format_supported() ? yes : no)
                .arg(floppy_format_supported()
                     ? tr("disponible")
                     : tr("no disponible en este sistema"));

    html += QStringLiteral("</ul>");
    html += tr("<p style='color:gray'>Para usar la disquetera sin ser root, "
               "añade tu usuario al grupo <code>floppy</code> (disquetera "
               "interna) o <code>disk</code> (USB) y vuelve a iniciar sesión:"
               "<br><code>sudo usermod -aG floppy $USER</code></p>");

    QMessageBox box(this);
    box.setWindowTitle(tr("Comprobar herramientas"));
    box.setTextFormat(Qt::RichText);
    box.setText(html);
    box.exec();
}

void MainWindow::showHowTo() {
    QString html = tr(
        "<h3>Cómo empezar</h3>"
        "<p>COPIA720 trabaja con disquetes de 3½ de dos maneras, que eliges "
        "arriba en «Usar:»</p>"
        "<ul>"
        "<li><b>Disquetera clásica (FDC):</b> la disquetera conectada al "
        "controlador de la placa base (<code>/dev/fd0</code>). Permite leer, "
        "grabar, explorar y <i>formatear a bajo nivel</i>.</li>"
        "<li><b>Greaseweazle (USB):</b> el dispositivo Greaseweazle conectado "
        "por USB. Permite leer, grabar y explorar en equipos modernos sin "
        "controlador de disquete. No formatea a bajo nivel: para dejar un "
        "disquete limpio, se graba una imagen ya formateada.</li>"
        "</ul>"
        "<table border='0' cellspacing='6'>"
        "<tr><td></td><td><b>FDC</b></td><td><b>Greaseweazle</b></td></tr>"
        "<tr><td>Crear imagen (leer)</td><td>✓</td><td>✓</td></tr>"
        "<tr><td>Grabar imagen</td><td>✓</td><td>✓</td></tr>"
        "<tr><td>Explorar FAT12</td><td>✓</td><td>✓</td></tr>"
        "<tr><td>Formatear bajo nivel</td><td>✓</td><td>✗</td></tr>"
        "</table>"
        "<p style='color:gray'>La Greaseweazle requiere la herramienta «gw». "
        "Comprueba su estado en Ayuda → Comprobar herramientas.</p>");

    QMessageBox box(this);
    box.setWindowTitle(tr("Cómo empezar"));
    box.setTextFormat(Qt::RichText);
    box.setText(html);
    box.exec();
}

void MainWindow::showAbout() {
#ifndef COPIA720_VERSION
#define COPIA720_VERSION "desconocida"
#endif
    QMessageBox::about(this, tr("Acerca de COPIA720"),
        tr("<h3>COPIA720 <span style='color:gray'>v%1</span></h3>"
           "<p>Guardar, restaurar y explorar disquetes de 3½ (720 KB y "
           "1.44 MB), con FAT12.</p>"
           "<p>Reescritura moderna del COPIA720 de F.J. Martos (1995), con "
           "núcleo en C y interfaz Qt. Soporta disquetera clásica (FDC) y "
           "Greaseweazle.</p>")
        .arg(QStringLiteral(COPIA720_VERSION)));
}
