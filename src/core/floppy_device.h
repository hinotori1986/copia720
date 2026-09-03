/* floppy_device.h — Lectura y escritura de un disquete físico completo.
 *
 * Sustituto moderno de las rutinas LeeSectores / GrabaPista del COPIA720.C
 * original: en lugar de programar el controlador con INT 13h, en Linux el
 * disquete es un dispositivo de bloque (/dev/fd0) y basta con leer o
 * escribir sus bytes en crudo.
 *
 * Geometría estándar del disquete de 3½ de 720 KB (doble cara, doble
 * densidad), que es el formato que manejaba el COPIA720 original:
 *   80 cilindros × 2 cabezas × 9 sectores × 512 bytes = 737 280 bytes.
 * El de 1.44 MB usa 18 sectores por pista.
 */
#ifndef FLOPPY_DEVICE_H
#define FLOPPY_DEVICE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Geometría de un formato de disquete. */
typedef struct {
    const char *label;     /* "720 KB", "1.44 MB" */
    int cylinders;         /* pistas por cara (80) */
    int heads;             /* caras (2) */
    int sectors;           /* sectores por pista (9 ó 18) */
    int sector_size;       /* bytes por sector (512) */
} FloppyGeometry;

/* Formatos predefinidos. */
extern const FloppyGeometry FLOPPY_720;
extern const FloppyGeometry FLOPPY_1440;

/* Bytes totales de un formato. */
size_t floppy_total_bytes(const FloppyGeometry *g);

/* Callback de progreso: se invoca tras cada pista leída/escrita. `done` y
 * `total` van en bytes. Devolver false CANCELA la operación en curso.
 * Puede ser NULL. `user` es un puntero opaco del llamante. */
typedef bool (*FloppyProgressFn)(size_t done, size_t total, void *user);

/* Resultado de una operación sobre el dispositivo. */
typedef enum {
    FLOPPY_OK = 0,
    FLOPPY_ERR_OPEN     = -1,   /* no se pudo abrir el dispositivo */
    FLOPPY_ERR_IO       = -2,   /* error de lectura/escritura */
    FLOPPY_ERR_NOMEM    = -3,
    FLOPPY_ERR_CANCELLED= -4,   /* el callback pidió cancelar */
    FLOPPY_ERR_SIZE     = -5,   /* tamaño de imagen incompatible */
    FLOPPY_ERR_BAD_PARAM= -6,
} FloppyStatus;

/* Lee el disquete entero de `device` (p. ej. "/dev/fd0") a un buffer recién
 * asignado del tamaño de la geometría. El llamante libera con free().
 * La lectura se hace pista a pista, reintentando cada una hasta `retries`
 * veces (como el LeeSectores original, que reintentaba con recalibración).
 * En error devuelve NULL y escribe el código en *status si no es NULL. */
uint8_t *floppy_read_image(const char *device, const FloppyGeometry *g,
                           int retries, FloppyProgressFn progress, void *user,
                           int *status);

/* Escribe `data`/`len` sobre el disquete `device`. `len` debe coincidir con
 * el tamaño de la geometría. Escribe pista a pista con reintentos. */
int floppy_write_image(const char *device, const FloppyGeometry *g,
                       const uint8_t *data, size_t len, int retries,
                       FloppyProgressFn progress, void *user);

/* Verifica que el contenido del disquete coincide con `data`/`len`,
 * leyéndolo de nuevo y comparando. Devuelve FLOPPY_OK si son idénticos,
 * FLOPPY_ERR_IO si difieren. Equivale a la opción /V del original. */
int floppy_verify_image(const char *device, const FloppyGeometry *g,
                        const uint8_t *data, size_t len,
                        FloppyProgressFn progress, void *user);

/* Traduce un código FloppyStatus a un mensaje en español. */
const char *floppy_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif /* FLOPPY_DEVICE_H */
