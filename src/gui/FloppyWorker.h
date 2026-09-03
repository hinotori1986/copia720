/* FloppyWorker.h — Tareas de larga duración sobre el disquete, en un hilo
 * aparte para no congelar la interfaz (mismo patrón que _WriteThread de
 * write_image_dialog.py).
 *
 * Cada worker envuelve una función del núcleo C y emite señales de progreso
 * y de finalización. La cancelación se hace mediante un flag atómico que el
 * callback de progreso del núcleo consulta.
 */
#ifndef FLOPPYWORKER_H
#define FLOPPYWORKER_H

#include <QThread>
#include <QString>
#include <atomic>
#include <cstdint>
#include <vector>

extern "C" {
#include "floppy_device.h"
}

/* Backend de acceso al disquete. */
enum class FloppyBackend {
    Fdc,          // controlador de disquete de la placa (/dev/fdX)
    Greaseweazle, // dispositivo USB Greaseweazle, vía la herramienta `gw`
};

/* Tipo de operación que ejecuta el worker. */
enum class FloppyOp {
    ReadImage,      // disquete → memoria
    WriteImage,     // memoria → disquete
    FormatDisk,     // formateo de bajo nivel
    WriteAndFormat, // formatear cada pista y luego escribir sus datos
};

class FloppyWorker : public QThread {
    Q_OBJECT
public:
    /* Para ReadImage: data vacío; al terminar, result() devuelve lo leído.
     * Para WriteImage / WriteAndFormat: data es la imagen a grabar.
     * Para FormatDisk: data vacío.
     * `backend` decide si se usa la FDC o la Greaseweazle. Para Greaseweazle,
     * `device` se ignora (la herramienta gw localiza el dispositivo USB). */
    FloppyWorker(FloppyOp op, QString device, FloppyGeometry geom,
                 std::vector<uint8_t> data = {}, bool verify = false,
                 FloppyBackend backend = FloppyBackend::Fdc,
                 QObject *parent = nullptr);

    void requestCancel() { cancel_.store(true); }

    /* Válido tras finalizar una ReadImage con éxito. */
    const std::vector<uint8_t> &result() const { return data_; }

signals:
    void progress(qint64 done, qint64 total);
    void finishedOk(bool ok, const QString &message);

protected:
    void run() override;

private:
    /* Puente para el callback C de progreso. */
    static bool progressTrampoline(size_t done, size_t total, void *user);

    FloppyOp op_;
    QString device_;
    FloppyGeometry geom_;
    std::vector<uint8_t> data_;
    bool verify_;
    FloppyBackend backend_;
    std::atomic<bool> cancel_{false};
};

#endif // FLOPPYWORKER_H
