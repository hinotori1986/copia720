/* floppy_format.c — ver floppy_format.h.
 *
 * Portado conceptual de FormateaPista (COPIA720.C). El original, sobre la
 * BIOS del PC, construía a mano el array de marcas de dirección {C,H,R,N} de
 * cada sector y lo pasaba a INT 13h/AH=05. En Linux la interfaz es distinta
 * y de más alto nivel: el driver de disquete ya conoce el layout de sectores
 * del formato seleccionado en el dispositivo, y el ioctl FDFMTTRK sólo recibe
 * qué pista formatear:
 *
 *     struct format_descr { unsigned int device, head, track; };
 *
 * El kernel se encarga de escribir las marcas de dirección de los sectores de
 * esa pista con el entrelazado estándar. Por eso el «sector sliding» del
 * original —un ajuste de entrelazado para el hardware de la época— no se
 * traslada literalmente: en una disquetera moderna no aporta nada y la
 * interfaz de Linux no lo admite. Se mantiene el parámetro por compatibilidad
 * de la API, pero se ignora.
 *
 * La secuencia correcta es FDFMTBEG (una vez), FDFMTTRK (por cada pista) y
 * FDFMTEND (una vez), tal como documenta el driver.
 */
#include "floppy_format.h"

#include <string.h>

#if defined(__linux__)
  #include <errno.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/ioctl.h>
  #include <linux/fd.h>
  #define FLOPPY_FORMAT_LINUX 1
#endif

bool floppy_format_supported(void) {
#ifdef FLOPPY_FORMAT_LINUX
    return true;
#else
    return false;
#endif
}

#ifdef FLOPPY_FORMAT_LINUX

int floppy_format_track(int fd, const FloppyGeometry *g, int cylinder,
                       int head, bool apply_sliding) {
    (void)apply_sliding;   /* no aplicable en la interfaz de Linux (ver arriba) */
    if (fd < 0 || !g)
        return FMT_ERR_BAD_PARAM;

    struct format_descr fd_descr;
    memset(&fd_descr, 0, sizeof fd_descr);
    /* `track` es el cilindro; `head` la cara. `device` se deja en 0: el
     * dispositivo ya está determinado por el descriptor abierto. */
    fd_descr.device = 0;
    fd_descr.head   = (unsigned)head;
    fd_descr.track  = (unsigned)cylinder;

    for (;;) {
        if (ioctl(fd, FDFMTTRK, &fd_descr) >= 0)
            return FMT_OK;
        if (errno == EINTR)
            continue;
        return FMT_ERR_IOCTL;
    }
}

int floppy_format_disk(const char *device, const FloppyGeometry *g,
                       bool apply_sliding, FloppyProgressFn progress,
                       void *user) {
    if (!device || !g)
        return FMT_ERR_BAD_PARAM;

    int fd = open(device, O_WRONLY);
    if (fd < 0)
        return FMT_ERR_OPEN;

    /* FDFMTBEG prepara el formateo. En algunos kernels no es imprescindible,
     * así que un fallo aquí no se considera fatal por sí solo. */
    ioctl(fd, FDFMTBEG, NULL);

    int st = FMT_OK;
    size_t total = floppy_total_bytes(g);
    size_t track_bytes = (size_t)g->sectors * g->sector_size;
    size_t done = 0;

    for (int cyl = 0; cyl < g->cylinders && st == FMT_OK; cyl++) {
        for (int head = 0; head < g->heads; head++) {
            int rc = floppy_format_track(fd, g, cyl, head, apply_sliding);
            if (rc != FMT_OK) {
                st = rc;
                break;
            }
            done += track_bytes;
            if (progress && !progress(done, total, user)) {
                st = FMT_ERR_CANCELLED;
                break;
            }
        }
    }

    ioctl(fd, FDFMTEND, NULL);
    close(fd);
    return st;
}

#else  /* plataforma no Linux: sin formateo de bajo nivel */

int floppy_format_track(int fd, const FloppyGeometry *g, int cylinder,
                       int head, bool apply_sliding) {
    (void)fd; (void)g; (void)cylinder; (void)head; (void)apply_sliding;
    return FMT_ERR_UNSUPPORTED;
}

int floppy_format_disk(const char *device, const FloppyGeometry *g,
                       bool apply_sliding, FloppyProgressFn progress,
                       void *user) {
    (void)device; (void)g; (void)apply_sliding; (void)progress; (void)user;
    return FMT_ERR_UNSUPPORTED;
}

#endif

const char *floppy_format_strerror(int status) {
    switch (status) {
    case FMT_OK:              return "formateo completado";
    case FMT_ERR_OPEN:        return "no se pudo abrir la disquetera para formatear";
    case FMT_ERR_IOCTL:       return "el controlador rechazó la orden de formateo";
    case FMT_ERR_CANCELLED:   return "formateo cancelado";
    case FMT_ERR_UNSUPPORTED: return "el formateo de bajo nivel no está disponible en este sistema";
    case FMT_ERR_BAD_PARAM:   return "parámetros de formateo incorrectos";
    default:                  return "error de formateo desconocido";
    }
}
