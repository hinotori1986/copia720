/* disk_image.h — Guardar y cargar imágenes de disquete en archivos.
 *
 * Equivalente moderno del «guardar disco en fichero» y «grabar fichero en
 * disco» del COPIA720 original, pero desacoplado del hardware: aquí sólo se
 * maneja el archivo .img en disco duro. La transferencia a/desde el disquete
 * físico la hace floppy_device.
 */
#ifndef DISK_IMAGE_H
#define DISK_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IMG_OK = 0,
    IMG_ERR_OPEN   = -1,
    IMG_ERR_IO     = -2,
    IMG_ERR_NOMEM  = -3,
    IMG_ERR_PARAM  = -4,
} ImageStatus;

/* Carga un archivo completo en memoria. Devuelve buffer asignado (liberar
 * con free) y su longitud en *out_len. NULL en error, con código en *status
 * si no es NULL. */
uint8_t *image_load(const char *path, size_t *out_len, int *status);

/* Guarda `data`/`len` en `path`, sobrescribiendo. Devuelve IMG_OK o error. */
int image_save(const char *path, const uint8_t *data, size_t len);

const char *image_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif /* DISK_IMAGE_H */
