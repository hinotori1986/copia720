/* fat12.c — implementación. Ver fat12.h.
 *
 * Portado de las funciones parse_dsk / parse_dir_entries / _fat_entry /
 * reconstruct_dsk_clusters / _parse_subdir / reconstruct_dsk_file de
 * rom_formats.py.
 */
#include "fat12.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Profundidad y longitud de cadena máximas: cortafuegos ante imágenes
 * corruptas o con ciclos en la FAT, igual que en el original Python. */
#define MAX_SUBDIR_DEPTH 16
#define MAX_CHAIN_GUARD  4096

/* --- lectura de enteros little-endian sin asumir alineación --- */
static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Copia `length` bytes desde data+offset como texto ASCII imprimible,
 * recortando espacios finales. Los no imprimibles se sustituyen por '.'.
 * Escribe en dst (que debe tener al menos length+1 bytes). */
static void ascii_field(const uint8_t *data, size_t off, size_t length,
                        char *dst) {
    size_t w = 0;
    for (size_t i = 0; i < length; i++) {
        uint8_t b = data[off + i];
        dst[w++] = (b >= 32 && b < 127) ? (char)b : '.';
    }
    /* recortar espacios (y puntos-relleno finales no, sólo espacios) al final */
    while (w > 0 && dst[w - 1] == ' ')
        w--;
    dst[w] = '\0';
}

/* Valor de la entrada n de la FAT de 12 bits. */
static uint16_t fat_entry(const uint8_t *fat, size_t fat_len, uint16_t n) {
    size_t off = ((size_t)n * 3) / 2;
    if (off + 1 >= fat_len)
        return 0xFFF;
    if (n % 2 == 0)
        return (uint16_t)(fat[off] | ((fat[off + 1] & 0x0F) << 8));
    return (uint16_t)((fat[off] >> 4) | (fat[off + 1] << 4));
}

/* Reconstruye los bytes de la cadena de clústeres desde start_cluster, sin
 * truncar por tamaño. Devuelve buffer asignado y su longitud en *out_len. */
static uint8_t *reconstruct_clusters(const Fat12Image *img,
                                     uint16_t start_cluster,
                                     size_t *out_len) {
    const uint8_t *fat = img->raw + (size_t)img->fat_start * img->bps;
    size_t fat_len = (size_t)img->spf * img->bps;
    size_t cluster_bytes = (size_t)img->spc * img->bps;

    /* Primera pasada: recorrer la cadena para saber cuántos clústeres hay. */
    uint16_t *chain = NULL;
    size_t chain_n = 0, chain_cap = 0;
    uint16_t cur = start_cluster;
    int guard = 0;
    while (cur >= 2 && cur < 0xFF0 && guard < MAX_CHAIN_GUARD) {
        if (chain_n == chain_cap) {
            size_t nc = chain_cap ? chain_cap * 2 : 32;
            uint16_t *tmp = realloc(chain, nc * sizeof *tmp);
            if (!tmp) { free(chain); return NULL; }
            chain = tmp;
            chain_cap = nc;
        }
        chain[chain_n++] = cur;
        cur = fat_entry(fat, fat_len, cur);
        guard++;
    }

    size_t total = chain_n * cluster_bytes;
    uint8_t *out = calloc(total ? total : 1, 1);
    if (!out) { free(chain); return NULL; }

    size_t p = 0;
    for (size_t i = 0; i < chain_n; i++) {
        uint16_t c = chain[i];
        size_t sector = img->data_start + (size_t)(c - 2) * img->spc;
        size_t start = sector * img->bps;
        if (start + cluster_bytes <= img->raw_len)
            memcpy(out + p, img->raw + start, cluster_bytes);
        /* si el clúster cae fuera de la imagen, se deja a cero (calloc) */
        p += cluster_bytes;
    }
    free(chain);
    *out_len = total;
    return out;
}

/* Declaración adelantada para la recursión de subdirectorios. */
static int parse_dir_entries(const Fat12Image *img, const uint8_t *raw,
                             size_t raw_len, int depth,
                             Fat12Entry **out, size_t *out_n);

/* Parsea el subdirectorio cuyo primer clúster es start_cluster. */
static int parse_subdir(const Fat12Image *img, uint16_t start_cluster,
                        int depth, Fat12Entry **out, size_t *out_n) {
    *out = NULL;
    *out_n = 0;
    if (depth > MAX_SUBDIR_DEPTH || start_cluster < 2)
        return FAT12_OK;
    size_t raw_len = 0;
    uint8_t *raw = reconstruct_clusters(img, start_cluster, &raw_len);
    if (!raw)
        return FAT12_ERR_NOMEM;
    int rc = parse_dir_entries(img, raw, raw_len, depth, out, out_n);
    free(raw);
    return rc;
}

/* Interpreta un bloque de entradas de directorio de 32 bytes. */
static int parse_dir_entries(const Fat12Image *img, const uint8_t *raw,
                             size_t raw_len, int depth,
                             Fat12Entry **out, size_t *out_n) {
    Fat12Entry *list = NULL;
    size_t n = 0, cap = 0;

    for (size_t off = 0; off + 32 <= raw_len; off += 32) {
        uint8_t b0 = raw[off];
        if (b0 == 0x00)
            break;              /* fin del directorio */
        if (b0 == 0xE5)
            continue;           /* entrada borrada */
        uint8_t attr = raw[off + 11];
        if (attr & 0x08)
            continue;           /* etiqueta de volumen */
        if (attr == 0x0F)
            continue;           /* entrada VFAT de nombre largo */

        char fname[9], fext[4];
        ascii_field(raw, off, 8, fname);
        ascii_field(raw, off + 8, 3, fext);
        if (fname[0] == '\0')
            continue;
        if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)
            continue;           /* entradas administrativas */

        if (n == cap) {
            size_t nc = cap ? cap * 2 : 16;
            Fat12Entry *tmp = realloc(list, nc * sizeof *tmp);
            if (!tmp) { free(list); return FAT12_ERR_NOMEM; }
            list = tmp;
            cap = nc;
        }
        Fat12Entry *e = &list[n];
        memset(e, 0, sizeof *e);

        /* nombre.ext */
        if (fext[0]) {
            snprintf(e->name, sizeof e->name, "%s.%s", fname, fext);
        } else {
            snprintf(e->name, sizeof e->name, "%s", fname);
        }
        e->attr    = attr;
        e->cluster = rd16(raw + off + 26);
        e->size    = rd32(raw + off + 28);
        e->is_dir  = (attr & 0x10) != 0;

        if (e->is_dir) {
            /* Un fallo de memoria al leer un subdirectorio no invalida el
             * resto del listado: la entrada queda como directorio vacío
             * (children = NULL). La imagen sigue siendo explorable y
             * fat12_free se encargará de liberar todo el árbol. */
            (void)parse_subdir(img, e->cluster, depth + 1,
                               &e->children, &e->child_count);
        }
        n++;
    }

    *out = list;
    *out_n = n;
    return FAT12_OK;
}

int fat12_parse(const uint8_t *data, size_t len, Fat12Image *out) {
    if (!data || !out)
        return FAT12_ERR_BAD_PARAM;
    if (len < 512)
        return FAT12_ERR_TOO_SMALL;

    memset(out, 0, sizeof *out);

    /* copia interna de los bytes crudos */
    out->raw = malloc(len);
    if (!out->raw)
        return FAT12_ERR_NOMEM;
    memcpy(out->raw, data, len);
    out->raw_len = len;

    const uint8_t *d = out->raw;

    /* BPB, con los mismos valores por defecto que el Python cuando el campo
     * viene a cero (imágenes .dsk «peladas» sin BPB completo). */
    out->bps          = rd16(d + 0x0B); if (!out->bps) out->bps = 512;
    out->spc          = d[0x0D];        if (!out->spc) out->spc = 2;
    out->reserved     = rd16(d + 0x0E); if (!out->reserved) out->reserved = 1;
    out->nfat         = d[0x10];        if (!out->nfat) out->nfat = 2;
    out->root_entries = rd16(d + 0x11); if (!out->root_entries) out->root_entries = 112;
    out->total_sectors= rd16(d + 0x13);
    out->media        = d[0x15];
    out->spf          = rd16(d + 0x16); if (!out->spf) out->spf = 3;

    out->fat_start  = out->reserved;
    out->root_start = out->reserved + (uint32_t)out->nfat * out->spf;
    uint32_t root_sectors =
        ((uint32_t)out->root_entries * 32 + out->bps - 1) / out->bps;
    out->data_start = out->root_start + root_sectors;

    /* directorio raíz */
    size_t root_off = (size_t)out->root_start * out->bps;
    size_t root_bytes = (size_t)out->root_entries * 32;
    if (root_off + root_bytes > len)
        root_bytes = (root_off < len) ? len - root_off : 0;

    int rc = parse_dir_entries(out, d + root_off, root_bytes, 0,
                               &out->entries, &out->entry_count);
    if (rc != FAT12_OK) {
        fat12_free(out);
        return rc;
    }
    return FAT12_OK;
}

/* Liberación recursiva de un array de entradas. */
static void free_entries(Fat12Entry *entries, size_t n) {
    if (!entries)
        return;
    for (size_t i = 0; i < n; i++) {
        if (entries[i].children)
            free_entries(entries[i].children, entries[i].child_count);
    }
    free(entries);
}

void fat12_free(Fat12Image *img) {
    if (!img)
        return;
    free_entries(img->entries, img->entry_count);
    free(img->raw);
    memset(img, 0, sizeof *img);
}

uint8_t *fat12_read_file(const Fat12Image *img, const Fat12Entry *entry,
                         size_t *out_len) {
    if (!img || !entry || !out_len)
        return NULL;
    size_t raw_len = 0;
    uint8_t *raw = reconstruct_clusters(img, entry->cluster, &raw_len);
    if (!raw)
        return NULL;
    size_t size = entry->size ? entry->size : raw_len;
    if (size > raw_len)
        size = raw_len;
    /* recortar el buffer al tamaño real */
    uint8_t *shrunk = realloc(raw, size ? size : 1);
    if (shrunk)
        raw = shrunk;
    *out_len = size;
    return raw;
}

void fat12_count(const Fat12Entry *entries, size_t n,
                 size_t *out_files, size_t *out_dirs) {
    size_t files = 0, dirs = 0;
    for (size_t i = 0; i < n; i++) {
        if (entries[i].is_dir) {
            dirs++;
            size_t f2 = 0, d2 = 0;
            fat12_count(entries[i].children, entries[i].child_count, &f2, &d2);
            files += f2;
            dirs  += d2;
        } else {
            files++;
        }
    }
    *out_files = files;
    *out_dirs = dirs;
}
