/* FloppyWorker.cpp — ver FloppyWorker.h. */
#include "FloppyWorker.h"

#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>   // free

extern "C" {
#include "floppy_format.h"
#include "floppy_greaseweazle.h"
}

FloppyWorker::FloppyWorker(FloppyOp op, QString device, FloppyGeometry geom,
                           std::vector<uint8_t> data, bool verify,
                           FloppyBackend backend, QObject *parent)
    : QThread(parent), op_(op), device_(std::move(device)), geom_(geom),
      data_(std::move(data)), verify_(verify), backend_(backend) {}

bool FloppyWorker::progressTrampoline(size_t done, size_t total, void *user) {
    auto *self = static_cast<FloppyWorker *>(user);
    emit self->progress(static_cast<qint64>(done), static_cast<qint64>(total));
    return !self->cancel_.load();   // false → el núcleo cancela
}

/* Escribe la imagen formateando cada pista justo antes de grabarla. Es el
 * equivalente moderno de la opción del COPIA720 original que iba formateando
 * mientras copiaba (y que reintentaba con formateo si la escritura fallaba).
 * Se hace aquí, en el worker, porque combina dos módulos del núcleo. */
static int writeAndFormat(const char *device, const FloppyGeometry *g,
                          const std::vector<uint8_t> &data,
                          FloppyProgressFn progress, void *user) {
    size_t total = floppy_total_bytes(g);
    if (data.size() != total)
        return FLOPPY_ERR_SIZE;

    int fd = open(device, O_WRONLY);
    if (fd < 0)
        return FLOPPY_ERR_OPEN;

    size_t track_bytes = (size_t)g->sectors * g->sector_size;
    size_t done = 0;
    int st = FLOPPY_OK;

    for (int cyl = 0; cyl < g->cylinders && st == FLOPPY_OK; cyl++) {
        for (int head = 0; head < g->heads; head++) {
            // 1) formatear la pista
            int frc = floppy_format_track(fd, g, cyl, head, /*sliding*/false);
            if (frc != FMT_OK) {
                st = FLOPPY_ERR_IO;
                break;
            }
            // 2) escribir sus datos
            off_t off = (off_t)done;
            if (lseek(fd, off, SEEK_SET) == (off_t)-1) { st = FLOPPY_ERR_IO; break; }
            size_t put = 0;
            const uint8_t *src = data.data() + off;
            while (put < track_bytes) {
                ssize_t w = write(fd, src + put, track_bytes - put);
                if (w < 0) { st = FLOPPY_ERR_IO; break; }
                put += (size_t)w;
            }
            if (st != FLOPPY_OK) break;

            done += track_bytes;
            if (progress && !progress(done, total, user)) {
                st = FLOPPY_ERR_CANCELLED;
                break;
            }
        }
    }

    if (st == FLOPPY_OK)
        fsync(fd);
    close(fd);
    return st;
}

void FloppyWorker::run() {
    QByteArray devBytes = device_.toUtf8();   // mantener vivo el buffer
    const char *dev = devBytes.constData();

    int st = FLOPPY_OK;
    QString msg;

    switch (op_) {
    case FloppyOp::ReadImage: {
        int status = 0;
        uint8_t *buf = nullptr;
        if (backend_ == FloppyBackend::Greaseweazle) {
            buf = gw_read_image(&geom_, progressTrampoline, this, &status);
        } else {
            buf = floppy_read_image(dev, &geom_, /*retries*/2,
                                    progressTrampoline, this, &status);
        }
        if (buf) {
            data_.assign(buf, buf + floppy_total_bytes(&geom_));
            free(buf);
            st = FLOPPY_OK;
        } else {
            st = status;
        }
        break;
    }
    case FloppyOp::WriteImage: {
        if (backend_ == FloppyBackend::Greaseweazle) {
            st = gw_write_image(&geom_, data_.data(), data_.size(),
                                progressTrampoline, this);
        } else {
            st = floppy_write_image(dev, &geom_, data_.data(), data_.size(),
                                    /*retries*/2, progressTrampoline, this);
        }
        if (st == FLOPPY_OK && verify_ && backend_ == FloppyBackend::Fdc) {
            st = floppy_verify_image(dev, &geom_, data_.data(), data_.size(),
                                     progressTrampoline, this);
            if (st == FLOPPY_ERR_IO)
                msg = QStringLiteral("La verificación detectó diferencias entre "
                                     "la imagen y el disquete grabado.");
        }
        break;
    }
    case FloppyOp::WriteAndFormat: {
        st = writeAndFormat(dev, &geom_, data_, progressTrampoline, this);
        if (st == FLOPPY_OK && verify_) {
            st = floppy_verify_image(dev, &geom_, data_.data(), data_.size(),
                                     progressTrampoline, this);
            if (st == FLOPPY_ERR_IO)
                msg = QStringLiteral("La verificación detectó diferencias tras "
                                     "formatear y grabar.");
        }
        break;
    }
    case FloppyOp::FormatDisk: {
        st = floppy_format_disk(dev, &geom_, /*sliding*/false,
                                progressTrampoline, this);
        if (st != FMT_OK)
            msg = QString::fromUtf8(floppy_format_strerror(st));
        emit finishedOk(st == FMT_OK, msg.isEmpty()
                        ? QString::fromUtf8(floppy_format_strerror(st)) : msg);
        return;
    }
    }

    if (msg.isEmpty())
        msg = QString::fromUtf8(floppy_strerror(st));
    emit finishedOk(st == FLOPPY_OK, msg);
}
