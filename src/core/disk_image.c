/* disk_image.c — ver disk_image.h. */
#include "disk_image.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

uint8_t *image_load(const char *path, size_t *out_len, int *status) {
    int st = IMG_OK;
    uint8_t *buf = NULL;
    FILE *f = NULL;

    if (!path || !out_len) { st = IMG_ERR_PARAM; goto done; }

    f = fopen(path, "rb");
    if (!f) { st = IMG_ERR_OPEN; goto done; }

    if (fseek(f, 0, SEEK_END) != 0) { st = IMG_ERR_IO; goto done; }
    long size = ftell(f);
    if (size < 0) { st = IMG_ERR_IO; goto done; }
    rewind(f);

    buf = malloc((size_t)size ? (size_t)size : 1);
    if (!buf) { st = IMG_ERR_NOMEM; goto done; }

    size_t got = fread(buf, 1, (size_t)size, f);
    if (got != (size_t)size) { st = IMG_ERR_IO; free(buf); buf = NULL; goto done; }

    *out_len = (size_t)size;

done:
    if (f) fclose(f);
    if (status) *status = st;
    return buf;
}

int image_save(const char *path, const uint8_t *data, size_t len) {
    if (!path || (!data && len)) return IMG_ERR_PARAM;

    FILE *f = fopen(path, "wb");
    if (!f) return IMG_ERR_OPEN;

    int st = IMG_OK;
    if (len && fwrite(data, 1, len, f) != len)
        st = IMG_ERR_IO;

    if (fflush(f) != 0) st = IMG_ERR_IO;
    if (fclose(f) != 0) st = IMG_ERR_IO;
    return st;
}

const char *image_strerror(int status) {
    switch (status) {
    case IMG_OK:        return "sin errores";
    case IMG_ERR_OPEN:  return "no se pudo abrir el archivo de imagen";
    case IMG_ERR_IO:    return "error de lectura/escritura del archivo";
    case IMG_ERR_NOMEM: return "memoria insuficiente";
    case IMG_ERR_PARAM: return "parámetros incorrectos";
    default:            return "error desconocido";
    }
}
