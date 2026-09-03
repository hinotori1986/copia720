/* floppy_greaseweazle.h — Backend de disquete a través de una Greaseweazle.
 *
 * Alternativa a floppy_device (que usa el controlador de disquete de la placa,
 * /dev/fdX). La Greaseweazle es un dispositivo USB que lee/escribe disquetes a
 * nivel de flujo magnético, lo que permite usar disqueteras en equipos
 * modernos sin controlador FDC. Funciona en cualquier arquitectura y sistema.
 *
 * En vez de reimplementar el protocolo USB de la Greaseweazle (enorme y ya
 * resuelto), este backend orquesta la herramienta oficial `gw` por debajo,
 * igual que volumes.c usa lsblk/udisksctl. `gw` decodifica el disquete a una
 * imagen de sectores .img, que luego el núcleo FAT12 explora igual que
 * cualquier otra imagen.
 *
 * Requiere que `gw` esté instalada y en el PATH:
 *     pipx install git+https://github.com/keirf/greaseweazle@latest
 */
#ifndef FLOPPY_GREASEWEAZLE_H
#define FLOPPY_GREASEWEAZLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "floppy_device.h"   /* FloppyGeometry, FloppyProgressFn, FloppyStatus */

#ifdef __cplusplus
extern "C" {
#endif

/* ¿Está la herramienta `gw` disponible en el sistema? */
bool gw_available(void);

/* Escribe en `out` (tamaño out_len) la versión de `gw` detectada, o "" si no
 * se pudo determinar. Devuelve true si `gw` está disponible. */
bool gw_version(char *out, size_t out_len);

/* Nombre de formato de gw para una geometría dada: "ibm.720", "ibm.1440".
 * Devuelve NULL si la geometría no tiene un formato gw conocido. */
const char *gw_format_for(const FloppyGeometry *g);

/* Lee el disquete con la Greaseweazle a un buffer de imagen de sectores.
 * Internamente invoca `gw read --format=<fmt> <tmp>.img` y carga ese archivo.
 * Devuelve un buffer recién asignado (liberar con free) del tamaño de la
 * geometría, o NULL en error (con el código en *status si no es NULL).
 *
 * El progreso de `gw` es menos granular que el de la FDC: se informa de
 * inicio y fin, y de las líneas de pista que `gw` va emitiendo si se pueden
 * interpretar. */
uint8_t *gw_read_image(const FloppyGeometry *g, FloppyProgressFn progress,
                       void *user, int *status);

/* Graba `data`/`len` en el disquete con la Greaseweazle. Escribe los datos a
 * un .img temporal e invoca `gw write --format=<fmt> <tmp>.img`. `len` debe
 * coincidir con el tamaño de la geometría. */
int gw_write_image(const FloppyGeometry *g, const uint8_t *data, size_t len,
                   FloppyProgressFn progress, void *user);

#ifdef __cplusplus
}
#endif

#endif /* FLOPPY_GREASEWEAZLE_H */
