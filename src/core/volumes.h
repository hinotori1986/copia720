/* volumes.h — Detección de unidades donde leer/escribir imágenes, y montaje
 * de dispositivos extraíbles para explorarlos.
 *
 * Portado de volumes.py (AsturConsole). Igual que allí, la filosofía es de
 * seguridad: grabar en crudo sobre un dispositivo BORRA todo su contenido,
 * así que sólo se ofrecen como destino discos extraíbles y disqueteras, y
 * los discos grandes se marcan como peligrosos.
 *
 * Se apoya en herramientas del sistema (lsblk, udisksctl, pkexec), igual que
 * el original en Python. Si no están, se degrada con elegancia.
 */
#ifndef VOLUMES_H
#define VOLUMES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VOL_PATH_MAX   256
#define VOL_LABEL_MAX  256

/* Un destino de escritura candidato (disco completo). */
typedef struct {
    char     path[VOL_PATH_MAX];    /* /dev/fd0, /dev/sdb */
    uint64_t size_bytes;
    char     size_label[32];        /* "1.4M", "8G" */
    char     model[VOL_LABEL_MAX];
    bool     removable;
    char     mountpoint[VOL_PATH_MAX]; /* primer punto de montaje, o "" */
} WriteTarget;

/* Un volumen montado que se puede explorar por carpetas. */
typedef struct {
    char path[VOL_PATH_MAX];        /* dispositivo, o "" */
    char mountpoint[VOL_PATH_MAX];  /* carpeta donde está montado */
    char label[VOL_LABEL_MAX];
    char size_label[32];
    char fstype[32];
    bool removable;
} MountedVolume;

/* Límite de seguridad: nada mayor que esto se considera destino «seguro». */
#define SAFE_TARGET_MAX_BYTES ((uint64_t)4 * 1024 * 1024 * 1024)

/* ¿Este destino parece razonable para una imagen de disquete? */
bool write_target_looks_safe(const WriteTarget *t);
bool write_target_is_floppy(const WriteTarget *t);

/* Lista discos completos candidatos a recibir una imagen. Rellena un array
 * asignado por la función (liberar con free) y escribe su longitud en
 * *out_n. Devuelve 0 en éxito. */
int list_write_targets(WriteTarget **out, size_t *out_n);

/* Lista volúmenes actualmente montados que interesa explorar (excluye los
 * puntos de montaje internos del sistema). */
int list_mounted_volumes(MountedVolume **out, size_t *out_n);

/* ¿Está disponible udisksctl (para montar/desmontar sin root)? */
bool volumes_can_mount(void);

/* Monta un dispositivo con udisksctl. En éxito escribe el punto de montaje
 * resultante en `mountpoint_out` (tamaño mp_len) y devuelve true. */
bool volume_mount(const char *device_path, char *mountpoint_out, size_t mp_len,
                  char *err_out, size_t err_len);

/* Desmonta el dispositivo y sus particiones (necesario antes de escribir en
 * crudo). Devuelve true en éxito. */
bool volume_unmount(const char *device_path, char *err_out, size_t err_len);

#ifdef __cplusplus
}
#endif

#endif /* VOLUMES_H */
