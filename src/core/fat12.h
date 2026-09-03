/* fat12.h — Lectura de sistemas de archivos FAT12 (disquetes MSX-DOS/DOS).
 *
 * Portado de rom_formats.py (AsturConsole). C17 puro, sin dependencias de
 * la GUI: puede probarse y reutilizarse por separado.
 *
 * Objetivo: dado el contenido crudo de una imagen de disquete (720 KB o
 * 1.44 MB, FAT12), reconstruir el árbol de directorios y poder extraer el
 * contenido de cualquier archivo. Es lo que sostiene la función «explorar
 * disquete».
 */
#ifndef FAT12_H
#define FAT12_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Longitud máxima de un nombre 8.3 más el punto y el terminador. */
#define FAT12_NAME_MAX 13

/* Una entrada de directorio ya interpretada. Los subdirectorios cuelgan de
 * `children` (un array asignado dinámicamente de longitud `child_count`). */
typedef struct Fat12Entry {
    char     name[FAT12_NAME_MAX]; /* "NOMBRE.EXT" en mayúsculas */
    uint8_t  attr;                 /* byte de atributos FAT (0x10 = dir, ...) */
    uint16_t cluster;              /* primer clúster de la cadena */
    uint32_t size;                 /* tamaño en bytes (0 para directorios) */
    bool     is_dir;

    struct Fat12Entry *children;   /* hijos, sólo si is_dir */
    size_t   child_count;
} Fat12Entry;

/* Imagen FAT12 ya analizada. Guarda una copia de los bytes crudos, de modo
 * que el llamante puede liberar su propio buffer tras parsear. */
typedef struct {
    uint16_t bps;            /* bytes por sector */
    uint8_t  spc;            /* sectores por clúster */
    uint16_t reserved;       /* sectores reservados */
    uint8_t  nfat;           /* número de FATs */
    uint16_t root_entries;   /* entradas del directorio raíz */
    uint16_t total_sectors;  /* sectores totales */
    uint8_t  media;          /* descriptor de medio */
    uint16_t spf;            /* sectores por FAT */

    uint32_t fat_start;      /* sector de inicio de la 1ª FAT */
    uint32_t root_start;     /* sector de inicio del directorio raíz */
    uint32_t data_start;     /* sector de inicio del área de datos */

    Fat12Entry *entries;     /* entradas de la raíz */
    size_t      entry_count;

    uint8_t *raw;            /* copia de los bytes de la imagen */
    size_t   raw_len;
} Fat12Image;

/* Códigos de error de las funciones que devuelven int. 0 = éxito. */
typedef enum {
    FAT12_OK = 0,
    FAT12_ERR_TOO_SMALL   = -1,  /* imagen menor que un sector */
    FAT12_ERR_NOMEM       = -2,
    FAT12_ERR_BAD_PARAM   = -3,
} Fat12Status;

/* Analiza `data`/`len` como una imagen FAT12. Copia los bytes internamente.
 * En éxito rellena *out (que el llamante debe liberar con fat12_free) y
 * devuelve FAT12_OK. */
int fat12_parse(const uint8_t *data, size_t len, Fat12Image *out);

/* Libera todo lo asociado a una imagen analizada (incluida la copia cruda
 * y el árbol de entradas). Deja la estructura a cero. Seguro con NULL. */
void fat12_free(Fat12Image *img);

/* Reconstruye el contenido de un archivo siguiendo su cadena de clústeres,
 * truncando al tamaño declarado en la entrada. Devuelve un buffer recién
 * asignado (que el llamante libera con free) y escribe su longitud en
 * *out_len. Devuelve NULL si falla. */
uint8_t *fat12_read_file(const Fat12Image *img, const Fat12Entry *entry,
                         size_t *out_len);

/* Cuenta recursivamente archivos y subcarpetas a partir de un array de
 * entradas (por ejemplo img->entries). */
void fat12_count(const Fat12Entry *entries, size_t n,
                 size_t *out_files, size_t *out_dirs);

#ifdef __cplusplus
}
#endif

#endif /* FAT12_H */
