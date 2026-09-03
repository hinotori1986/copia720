/* volumes.c — ver volumes.h.
 *
 * Portado de volumes.py. En vez de la salida JSON de lsblk (que exigiría un
 * parser JSON), se usa el modo `-P` (pares CLAVE="valor" por línea), trivial
 * de analizar en C puro y estable entre versiones de util-linux.
 */
#include "volumes.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* Puntos de montaje internos que no interesa mostrar como explorables. */
static const char *HIDDEN_PREFIXES[] = {
    "/proc", "/sys", "/dev", "/run", "/snap", "/var/lib", "/var/snap",
    "/boot/efi", "/tmp", NULL
};

/* ---- utilidades de proceso -------------------------------------------- */

/* ¿Existe `prog` en el PATH? (equivalente a shutil.which) */
static bool have_command(const char *prog) {
    const char *path = getenv("PATH");
    if (!path) return false;
    char buf[VOL_PATH_MAX];
    const char *p = path;
    while (*p) {
        const char *colon = strchr(p, ':');
        size_t len = colon ? (size_t)(colon - p) : strlen(p);
        if (len && len + 1 + strlen(prog) + 1 < sizeof buf) {
            memcpy(buf, p, len);
            buf[len] = '/';
            strcpy(buf + len + 1, prog);
            if (access(buf, X_OK) == 0)
                return true;
        }
        if (!colon) break;
        p = colon + 1;
    }
    return false;
}

/* Ejecuta argv y captura su stdout en un buffer asignado (liberar con free).
 * Devuelve el código de salida, o -1 si no se pudo lanzar. */
static int run_capture(char *const argv[], char **out) {
    *out = NULL;
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]); close(pipefd[1]);
        return -1;
    }
    if (pid == 0) {
        /* hijo */
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        /* silenciar stderr para no ensuciar la GUI */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execvp(argv[0], argv);
        _exit(127);
    }

    /* padre */
    close(pipefd[1]);
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) { close(pipefd[0]); }
    else {
        for (;;) {
            if (len + 1 >= cap) {
                size_t nc = cap * 2;
                char *tmp = realloc(buf, nc);
                if (!tmp) { free(buf); buf = NULL; break; }
                buf = tmp; cap = nc;
            }
            ssize_t r = read(pipefd[0], buf + len, cap - len - 1);
            if (r < 0) { if (errno == EINTR) continue; break; }
            if (r == 0) break;
            len += (size_t)r;
        }
        if (buf) buf[len] = '\0';
        close(pipefd[0]);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    *out = buf;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* ---- parseo de lsblk -P ------------------------------------------------ */

/* Extrae el valor de CLAVE="valor" dentro de una línea de lsblk -P. Copia en
 * dst (tamaño dstlen). Devuelve true si encontró la clave. */
static bool kv_get(const char *line, const char *key, char *dst, size_t dstlen) {
    char needle[64];
    snprintf(needle, sizeof needle, "%s=\"", key);
    const char *p = strstr(line, needle);
    if (!p) { if (dstlen) dst[0] = '\0'; return false; }
    p += strlen(needle);
    size_t w = 0;
    while (*p && *p != '"' && w + 1 < dstlen)
        dst[w++] = *p++;
    if (dstlen) dst[w] = '\0';
    return true;
}

/* Convierte "1.4M", "720K", "8G" a bytes. */
static uint64_t parse_size(const char *s) {
    if (!s || !*s) return 0;
    double val = atof(s);
    size_t n = strlen(s);
    char suf = toupper((unsigned char)s[n - 1]);
    uint64_t mult = 1;
    switch (suf) {
        case 'K': mult = 1024ULL; break;
        case 'M': mult = 1024ULL * 1024; break;
        case 'G': mult = 1024ULL * 1024 * 1024; break;
        case 'T': mult = 1024ULL * 1024 * 1024 * 1024; break;
        default: break;
    }
    return (uint64_t)(val * (double)mult);
}

/* ---- API pública ------------------------------------------------------- */

bool write_target_is_floppy(const WriteTarget *t) {
    return t && strstr(t->path, "/dev/fd") != NULL;
}

bool write_target_looks_safe(const WriteTarget *t) {
    if (!t) return false;
    if (write_target_is_floppy(t)) return true;
    return t->removable && t->size_bytes > 0 &&
           t->size_bytes <= SAFE_TARGET_MAX_BYTES;
}

bool volumes_can_mount(void) {
    return have_command("udisksctl");
}

int list_write_targets(WriteTarget **out, size_t *out_n) {
    *out = NULL;
    *out_n = 0;
    if (!have_command("lsblk"))
        return 0;

    char *argv[] = {
        "lsblk", "-P", "-d", "-o",
        "PATH,SIZE,TYPE,RM,HOTPLUG,MODEL,MOUNTPOINT", NULL
    };
    char *stdout_buf = NULL;
    if (run_capture(argv, &stdout_buf) != 0 || !stdout_buf) {
        free(stdout_buf);
        return 0;
    }

    size_t cap = 8, n = 0;
    WriteTarget *list = malloc(cap * sizeof *list);
    if (!list) { free(stdout_buf); return -1; }

    char *save = NULL;
    for (char *line = strtok_r(stdout_buf, "\n", &save);
         line;
         line = strtok_r(NULL, "\n", &save)) {

        char type[16];
        kv_get(line, "TYPE", type, sizeof type);
        if (strcmp(type, "disk") != 0)
            continue;

        char path[VOL_PATH_MAX];
        if (!kv_get(line, "PATH", path, sizeof path) || !path[0])
            continue;

        char rm[8], hp[8], size[32], model[VOL_LABEL_MAX], mp[VOL_PATH_MAX];
        kv_get(line, "RM", rm, sizeof rm);
        kv_get(line, "HOTPLUG", hp, sizeof hp);
        kv_get(line, "SIZE", size, sizeof size);
        kv_get(line, "MODEL", model, sizeof model);
        kv_get(line, "MOUNTPOINT", mp, sizeof mp);

        if (n == cap) {
            size_t nc = cap * 2;
            WriteTarget *tmp = realloc(list, nc * sizeof *list);
            if (!tmp) { free(list); free(stdout_buf); return -1; }
            list = tmp; cap = nc;
        }
        WriteTarget *t = &list[n++];
        memset(t, 0, sizeof *t);
        snprintf(t->path, sizeof t->path, "%s", path);
        snprintf(t->size_label, sizeof t->size_label, "%s", size);
        t->size_bytes = parse_size(size);
        snprintf(t->model, sizeof t->model, "%s", model);
        t->removable = (rm[0] == '1') || (hp[0] == '1');
        snprintf(t->mountpoint, sizeof t->mountpoint, "%s", mp);
    }
    free(stdout_buf);

    /* Disqueteras clásicas, que lsblk no siempre lista. */
    for (int i = 0; i < 2; i++) {
        char dev[16];
        snprintf(dev, sizeof dev, "/dev/fd%d", i);
        if (access(dev, F_OK) != 0)
            continue;
        bool already = false;
        for (size_t k = 0; k < n; k++)
            if (strcmp(list[k].path, dev) == 0) { already = true; break; }
        if (already) continue;

        if (n == cap) {
            size_t nc = cap * 2;
            WriteTarget *tmp = realloc(list, nc * sizeof *list);
            if (!tmp) { free(list); return -1; }
            list = tmp; cap = nc;
        }
        WriteTarget *t = &list[n++];
        memset(t, 0, sizeof *t);
        snprintf(t->path, sizeof t->path, "%s", dev);
        snprintf(t->size_label, sizeof t->size_label, "1.4M");
        t->size_bytes = 1474560;
        snprintf(t->model, sizeof t->model, "disquetera");
        t->removable = true;
    }

    *out = list;
    *out_n = n;
    return 0;
}

static bool hidden_mount(const char *mp) {
    for (int i = 0; HIDDEN_PREFIXES[i]; i++)
        if (strncmp(mp, HIDDEN_PREFIXES[i], strlen(HIDDEN_PREFIXES[i])) == 0)
            return true;
    return false;
}

int list_mounted_volumes(MountedVolume **out, size_t *out_n) {
    *out = NULL;
    *out_n = 0;
    if (!have_command("lsblk"))
        return 0;

    char *argv[] = {
        "lsblk", "-P", "-o",
        "PATH,SIZE,TYPE,MOUNTPOINT,LABEL,RM,FSTYPE,HOTPLUG", NULL
    };
    char *stdout_buf = NULL;
    if (run_capture(argv, &stdout_buf) != 0 || !stdout_buf) {
        free(stdout_buf);
        return 0;
    }

    size_t cap = 8, n = 0;
    MountedVolume *list = malloc(cap * sizeof *list);
    if (!list) { free(stdout_buf); return -1; }

    char *save = NULL;
    for (char *line = strtok_r(stdout_buf, "\n", &save);
         line;
         line = strtok_r(NULL, "\n", &save)) {

        char mp[VOL_PATH_MAX];
        kv_get(line, "MOUNTPOINT", mp, sizeof mp);
        if (!mp[0] || hidden_mount(mp))
            continue;

        char path[VOL_PATH_MAX], size[32], label[VOL_LABEL_MAX];
        char rm[8], fstype[32], hp[8];
        kv_get(line, "PATH", path, sizeof path);
        kv_get(line, "SIZE", size, sizeof size);
        kv_get(line, "LABEL", label, sizeof label);
        kv_get(line, "RM", rm, sizeof rm);
        kv_get(line, "FSTYPE", fstype, sizeof fstype);
        kv_get(line, "HOTPLUG", hp, sizeof hp);

        if (n == cap) {
            size_t nc = cap * 2;
            MountedVolume *tmp = realloc(list, nc * sizeof *list);
            if (!tmp) { free(list); free(stdout_buf); return -1; }
            list = tmp; cap = nc;
        }
        MountedVolume *v = &list[n++];
        memset(v, 0, sizeof *v);
        snprintf(v->path, sizeof v->path, "%s", path);
        snprintf(v->mountpoint, sizeof v->mountpoint, "%s", mp);
        snprintf(v->label, sizeof v->label, "%s", label);
        snprintf(v->size_label, sizeof v->size_label, "%s", size);
        snprintf(v->fstype, sizeof v->fstype, "%s", fstype);
        v->removable = (rm[0] == '1') || (hp[0] == '1');
    }
    free(stdout_buf);

    *out = list;
    *out_n = n;
    return 0;
}

bool volume_mount(const char *device_path, char *mountpoint_out, size_t mp_len,
                  char *err_out, size_t err_len) {
    if (mp_len) mountpoint_out[0] = '\0';
    if (err_len) err_out[0] = '\0';

    if (!volumes_can_mount()) {
        snprintf(err_out, err_len,
                 "No se encontró 'udisksctl' (paquete udisks2), necesario para "
                 "montar sin privilegios de root.");
        return false;
    }

    char *argv[] = { "udisksctl", "mount", "-b", (char *)device_path, NULL };
    char *stdout_buf = NULL;
    int rc = run_capture(argv, &stdout_buf);

    bool ok = false;
    if (rc == 0 && stdout_buf) {
        /* Formato típico: "Mounted /dev/sdb1 at /media/user/ETIQUETA" */
        const char *marker = " at ";
        char *at = strstr(stdout_buf, marker);
        if (at) {
            at += strlen(marker);
            char *end = at + strlen(at);
            while (end > at && (end[-1] == '\n' || end[-1] == '.' ||
                                end[-1] == ' '))
                end--;
            size_t len = (size_t)(end - at);
            if (len && len < mp_len) {
                memcpy(mountpoint_out, at, len);
                mountpoint_out[len] = '\0';
                struct stat sb;
                if (stat(mountpoint_out, &sb) == 0 && S_ISDIR(sb.st_mode))
                    ok = true;
            }
        }
    }
    if (!ok && err_len)
        snprintf(err_out, err_len, "no se pudo montar %s", device_path);
    free(stdout_buf);
    return ok;
}

bool volume_unmount(const char *device_path, char *err_out, size_t err_len) {
    if (err_len) err_out[0] = '\0';
    if (!volumes_can_mount()) {
        snprintf(err_out, err_len, "no se encontró 'udisksctl' para desmontar");
        return false;
    }
    char *argv[] = { "udisksctl", "unmount", "-b", (char *)device_path, NULL };
    char *stdout_buf = NULL;
    int rc = run_capture(argv, &stdout_buf);
    bool ok = (rc == 0);
    if (!ok) {
        /* "not mounted" no es un error real para nuestro propósito */
        if (stdout_buf && strstr(stdout_buf, "not mounted"))
            ok = true;
        else if (err_len)
            snprintf(err_out, err_len, "no se pudo desmontar %s", device_path);
    }
    free(stdout_buf);
    return ok;
}
