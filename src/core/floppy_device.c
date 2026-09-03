/* floppy_device.c — ver floppy_device.h.
 *
 * Lectura/escritura pista a pista sobre /dev/fdX. El troceado por pistas no
 * es sólo cosmético: permite reintentar una pista concreta ante un error de
 * medio (como hacía el bucle de reintentos del COPIA720.C original) y da
 * granularidad al progreso.
 */
#include "floppy_device.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

const FloppyGeometry FLOPPY_720  = { "720 KB",  80, 2,  9, 512 };
const FloppyGeometry FLOPPY_1440 = { "1.44 MB", 80, 2, 18, 512 };

size_t floppy_total_bytes(const FloppyGeometry *g) {
    if (!g) return 0;
    return (size_t)g->cylinders * g->heads * g->sectors * g->sector_size;
}

/* Lee exactamente `count` bytes en `buf` desde el descriptor `fd` en la
 * posición actual, reintentando ante lecturas parciales. Devuelve 0 en
 * éxito, -1 en error de E/S. */
static int read_full(int fd, uint8_t *buf, size_t count) {
    size_t got = 0;
    while (got < count) {
        ssize_t r = read(fd, buf + got, count - got);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return -1;   /* EOF inesperado */
        got += (size_t)r;
    }
    return 0;
}

static int write_full(int fd, const uint8_t *buf, size_t count) {
    size_t put = 0;
    while (put < count) {
        ssize_t w = write(fd, buf + put, count - put);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        put += (size_t)w;
    }
    return 0;
}

/* Lee una pista (posición track_off, longitud track_bytes) reintentando. */
static int read_track_retry(int fd, off_t track_off, uint8_t *dst,
                            size_t track_bytes, int retries) {
    for (int attempt = 0; attempt <= retries; attempt++) {
        if (lseek(fd, track_off, SEEK_SET) == (off_t)-1)
            return -1;
        if (read_full(fd, dst, track_bytes) == 0)
            return 0;
        /* En un reintento conviene forzar reposicionamiento del cabezal;
         * el propio lseek de la siguiente vuelta lo hace. */
    }
    return -1;
}

static int write_track_retry(int fd, off_t track_off, const uint8_t *src,
                             size_t track_bytes, int retries) {
    for (int attempt = 0; attempt <= retries; attempt++) {
        if (lseek(fd, track_off, SEEK_SET) == (off_t)-1)
            return -1;
        if (write_full(fd, src, track_bytes) == 0)
            return 0;
    }
    return -1;
}

uint8_t *floppy_read_image(const char *device, const FloppyGeometry *g,
                           int retries, FloppyProgressFn progress, void *user,
                           int *status) {
    int st = FLOPPY_OK;
    uint8_t *buf = NULL;

    if (!device || !g) { st = FLOPPY_ERR_BAD_PARAM; goto done; }

    size_t total = floppy_total_bytes(g);
    size_t track_bytes = (size_t)g->sectors * g->sector_size;

    int fd = open(device, O_RDONLY);
    if (fd < 0) { st = FLOPPY_ERR_OPEN; goto done; }

    buf = malloc(total);
    if (!buf) { st = FLOPPY_ERR_NOMEM; close(fd); goto done; }

    size_t done_bytes = 0;
    int ntracks = g->cylinders * g->heads;
    for (int t = 0; t < ntracks; t++) {
        off_t off = (off_t)t * track_bytes;
        if (read_track_retry(fd, off, buf + off, track_bytes, retries) != 0) {
            st = FLOPPY_ERR_IO;
            break;
        }
        done_bytes += track_bytes;
        if (progress && !progress(done_bytes, total, user)) {
            st = FLOPPY_ERR_CANCELLED;
            break;
        }
    }
    close(fd);

    if (st != FLOPPY_OK) {
        free(buf);
        buf = NULL;
    }
done:
    if (status) *status = st;
    return buf;
}

int floppy_write_image(const char *device, const FloppyGeometry *g,
                       const uint8_t *data, size_t len, int retries,
                       FloppyProgressFn progress, void *user) {
    if (!device || !g || !data)
        return FLOPPY_ERR_BAD_PARAM;

    size_t total = floppy_total_bytes(g);
    if (len != total)
        return FLOPPY_ERR_SIZE;

    size_t track_bytes = (size_t)g->sectors * g->sector_size;

    int fd = open(device, O_WRONLY);
    if (fd < 0)
        return FLOPPY_ERR_OPEN;

    int st = FLOPPY_OK;
    size_t done_bytes = 0;
    int ntracks = g->cylinders * g->heads;
    for (int t = 0; t < ntracks; t++) {
        off_t off = (off_t)t * track_bytes;
        if (write_track_retry(fd, off, data + off, track_bytes, retries) != 0) {
            st = FLOPPY_ERR_IO;
            break;
        }
        done_bytes += track_bytes;
        if (progress && !progress(done_bytes, total, user)) {
            st = FLOPPY_ERR_CANCELLED;
            break;
        }
    }

    /* asegurar que todo baja al medio antes de cerrar */
    if (st == FLOPPY_OK)
        fsync(fd);
    close(fd);
    return st;
}

int floppy_verify_image(const char *device, const FloppyGeometry *g,
                        const uint8_t *data, size_t len,
                        FloppyProgressFn progress, void *user) {
    if (!device || !g || !data)
        return FLOPPY_ERR_BAD_PARAM;

    size_t total = floppy_total_bytes(g);
    if (len != total)
        return FLOPPY_ERR_SIZE;

    size_t track_bytes = (size_t)g->sectors * g->sector_size;

    int fd = open(device, O_RDONLY);
    if (fd < 0)
        return FLOPPY_ERR_OPEN;

    uint8_t *track = malloc(track_bytes);
    if (!track) { close(fd); return FLOPPY_ERR_NOMEM; }

    int st = FLOPPY_OK;
    size_t done_bytes = 0;
    int ntracks = g->cylinders * g->heads;
    for (int t = 0; t < ntracks; t++) {
        off_t off = (off_t)t * track_bytes;
        if (lseek(fd, off, SEEK_SET) == (off_t)-1 ||
            read_full(fd, track, track_bytes) != 0) {
            st = FLOPPY_ERR_IO;
            break;
        }
        if (memcmp(track, data + off, track_bytes) != 0) {
            st = FLOPPY_ERR_IO;   /* difieren */
            break;
        }
        done_bytes += track_bytes;
        if (progress && !progress(done_bytes, total, user)) {
            st = FLOPPY_ERR_CANCELLED;
            break;
        }
    }

    free(track);
    close(fd);
    return st;
}

const char *floppy_strerror(int status) {
    switch (status) {
    case FLOPPY_OK:            return "sin errores";
    case FLOPPY_ERR_OPEN:      return "no se pudo abrir la disquetera";
    case FLOPPY_ERR_IO:        return "error de lectura/escritura del disquete";
    case FLOPPY_ERR_NOMEM:     return "memoria insuficiente";
    case FLOPPY_ERR_CANCELLED: return "operación cancelada";
    case FLOPPY_ERR_SIZE:      return "el tamaño de la imagen no coincide con el disquete";
    case FLOPPY_ERR_BAD_PARAM: return "parámetros incorrectos";
    default:                   return "error desconocido";
    }
}
