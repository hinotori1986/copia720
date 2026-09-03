/* floppy_format.h — Formateo de bajo nivel de un disquete, pista a pista.
 *
 * Equivalente moderno de la función FormateaPista del COPIA720.C original,
 * que emitía la orden 0x05 (format track) del INT 13h. En Linux se hace con
 * los ioctls FDFMTBEG / FDFMTTRK / FDFMTEND del driver de disquete
 * (<linux/fd.h>), que existen precisamente para esto: escribir las marcas
 * de dirección (C, H, R, N) de cada sector de una pista, dejándola lista
 * para recibir datos. Es lo que hace falta para usar disquetes VÍRGENES,
 * que aún no tienen formato.
 *
 * Nota sobre el «sector sliding» del original: COPIA720 numeraba los
 * sectores de cada pista con un desfase (Cilindro%3)*3+Cabezal para escalonar
 * el entrelazado entre pistas y acelerar la lectura secuencial en el
 * hardware de la época. Se conserva ese mismo cálculo por fidelidad, aunque
 * en una disquetera USB moderna el efecto es irrelevante.
 *
 * IMPORTANTE: este módulo sólo compila y funciona en Linux. En otros
 * sistemas, las funciones devuelven FMT_ERR_UNSUPPORTED.
 */
#ifndef FLOPPY_FORMAT_H
#define FLOPPY_FORMAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "floppy_device.h"   /* FloppyGeometry, FloppyProgressFn */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FMT_OK = 0,
    FMT_ERR_OPEN        = -1,
    FMT_ERR_IOCTL       = -2,   /* falló un ioctl de formateo */
    FMT_ERR_CANCELLED   = -3,
    FMT_ERR_UNSUPPORTED = -4,   /* plataforma sin soporte de formateo */
    FMT_ERR_BAD_PARAM   = -5,
} FormatStatus;

/* ¿Está disponible el formateo de bajo nivel en esta compilación/plataforma? */
bool floppy_format_supported(void);

/* Formatea el disquete completo `device` con la geometría `g`, pista a
 * pista. Si `apply_sliding` es true, aplica el desfase de sectores del
 * COPIA720 original. `progress` se invoca tras cada pista; devolver false
 * cancela. Devuelve FMT_OK o un código de error. */
int floppy_format_disk(const char *device, const FloppyGeometry *g,
                       bool apply_sliding, FloppyProgressFn progress,
                       void *user);

/* Formatea UNA sola pista (cilindro, cabezal). Se expone por separado para
 * poder «formatear mientras se copia», como permitía el COPIA720 original:
 * la GUI puede formatear la pista justo antes de escribir sus datos. */
int floppy_format_track(int fd, const FloppyGeometry *g, int cylinder,
                       int head, bool apply_sliding);

/* Traduce un FormatStatus a mensaje en español. */
const char *floppy_format_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif /* FLOPPY_FORMAT_H */
