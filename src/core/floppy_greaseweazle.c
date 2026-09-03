/* floppy_greaseweazle.c — ver floppy_greaseweazle.h.
 *
 * Orquesta la herramienta `gw` mediante fork/exec, usando un archivo .img
 * temporal como puente:
 *   · leer:   gw read  --format=<fmt> <tmp>   →  cargar <tmp> en memoria
 *   · grabar: volcar memoria a <tmp>  →  gw write --format=<fmt> <tmp>
 *
 * El progreso de `gw` se sigue de forma aproximada: la herramienta emite por
 * su salida líneas por pista (p. ej. "T0.0: ..."), que se interpretan para
 * mover la barra. Si no se pueden interpretar, se informa igualmente de
 * inicio y fin.
 */
#include "floppy_greaseweazle.h"
#include "disk_image.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* --- utilidades de PATH (misma idea que en volumes.c) --------------------- */

static bool have_command(const char *prog) {
    const char *path = getenv("PATH");
    if (!path) return false;
    char buf[512];
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

bool gw_available(void) {
    return have_command("gw");
}

const char *gw_format_for(const FloppyGeometry *g) {
    if (!g) return NULL;
    size_t total = floppy_total_bytes(g);
    if (total == 737280) return "ibm.720";    /* 720 KB */
    if (total == 1474560) return "ibm.1440";  /* 1.44 MB */
    if (total == 368640) return "ibm.360";    /* 360 KB */
    if (total == 1228800) return "ibm.1200";  /* 1.2 MB */
    return NULL;
}

/* Ejecuta argv capturando stdout+stderr combinados. Llama a `on_line` por
 * cada línea recibida (para progreso). Devuelve el código de salida del
 * proceso, o -1 si no se pudo lanzar. Si on_line devuelve false, se aborta
 * (se mata el proceso hijo). */
typedef bool (*LineFn)(const char *line, void *user);

static int run_streaming(char *const argv[], LineFn on_line, void *user) {
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]); close(pipefd[1]);
        return -1;
    }
    if (pid == 0) {
        /* hijo: stdout y stderr al pipe */
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    /* padre: leer líneas del pipe */
    close(pipefd[1]);
    FILE *f = fdopen(pipefd[0], "r");
    bool aborted = false;
    if (f) {
        char line[512];
        while (fgets(line, sizeof line, f)) {
            /* recortar salto de línea */
            size_t n = strlen(line);
            while (n && (line[n-1] == '\n' || line[n-1] == '\r'))
                line[--n] = '\0';
            if (on_line && !on_line(line, user)) {
                aborted = true;
                kill(pid, SIGTERM);
                break;
            }
        }
        fclose(f);   /* cierra pipefd[0] */
    } else {
        close(pipefd[0]);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (aborted)
        return -2;   /* señal de cancelación */
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Contexto para capturar la primera línea no vacía. */
typedef struct { char *dst; size_t len; bool got; } FirstLineCtx;

static bool first_line_cb(const char *line, void *user) {
    FirstLineCtx *c = (FirstLineCtx *)user;
    if (!c->got && line[0] != '\0') {
        snprintf(c->dst, c->len, "%s", line);
        c->got = true;
    }
    return true;  /* seguir leyendo para drenar el resto de la salida */
}

bool gw_version(char *out, size_t out_len) {
    if (out_len) out[0] = '\0';
    if (!gw_available())
        return false;

    FirstLineCtx ctx = { out, out_len, false };
    char *argv[] = { "gw", "--version", NULL };
    run_streaming(argv, first_line_cb, &ctx);
    return true;  /* gw existe aunque no hayamos podido leer la versión */
}

/* --- contexto de progreso para lectura/escritura ------------------------- */

typedef struct {
    FloppyProgressFn progress;
    void *user;
    size_t total;
    int cyl_seen;      /* nº de cilindros vistos, para estimar avance */
    int cylinders;     /* cilindros totales de la geometría */
    bool cancelled;
} GwProgressCtx;

/* Interpreta líneas de `gw` para estimar el progreso. `gw` emite líneas por
 * pista tipo "T0.0", "T0.1", "T1.0"... Contamos cilindros distintos vistos. */
static bool gw_progress_line(const char *line, void *user) {
    GwProgressCtx *ctx = (GwProgressCtx *)user;

    /* Detectar un marcador de pista "T<cil>.<cara>". Es heurístico y tolerante:
     * si el formato de salida de gw cambia, simplemente el progreso será menos
     * fino, pero la operación sigue. */
    if (line[0] == 'T' && line[1] >= '0' && line[1] <= '9') {
        int cyl = atoi(line + 1);
        if (cyl + 1 > ctx->cyl_seen)
            ctx->cyl_seen = cyl + 1;
    }

    if (ctx->progress && ctx->total && ctx->cylinders > 0) {
        size_t done = (size_t)((double)ctx->cyl_seen / ctx->cylinders * ctx->total);
        if (done > ctx->total) done = ctx->total;
        if (!ctx->progress(done, ctx->total, ctx->user)) {
            ctx->cancelled = true;
            return false;   /* aborta run_streaming */
        }
    }
    return true;
}

/* Crea una ruta de archivo temporal para el .img puente. */
static bool make_temp_img(char *out, size_t out_len) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/tmp";
    snprintf(out, out_len, "%s/copia720_gw_XXXXXX.img", tmpdir);
    /* mkstemps no siempre disponible; usamos mkstemp sobre una copia sin el
     * sufijo y luego renombramos conceptualmente. Para simplicidad y
     * portabilidad, generamos con mkstemp y añadimos .img aparte. */
    char base[512];
    snprintf(base, sizeof base, "%s/copia720_gw_XXXXXX", tmpdir);
    int fd = mkstemp(base);
    if (fd < 0)
        return false;
    close(fd);
    unlink(base);              /* nos quedamos sólo con el nombre único */
    snprintf(out, out_len, "%s.img", base);
    return true;
}

uint8_t *gw_read_image(const FloppyGeometry *g, FloppyProgressFn progress,
                       void *user, int *status) {
    int st = FLOPPY_OK;
    uint8_t *result = NULL;
    char tmp[600];

    if (!g) { st = FLOPPY_ERR_BAD_PARAM; goto done; }
    if (!gw_available()) { st = FLOPPY_ERR_OPEN; goto done; }

    const char *fmt = gw_format_for(g);
    if (!fmt) { st = FLOPPY_ERR_BAD_PARAM; goto done; }

    if (!make_temp_img(tmp, sizeof tmp)) { st = FLOPPY_ERR_IO; goto done; }

    /* gw read --format=<fmt> <tmp> */
    char fmtarg[64];
    snprintf(fmtarg, sizeof fmtarg, "--format=%s", fmt);
    char *argv[] = { "gw", "read", fmtarg, tmp, NULL };

    GwProgressCtx ctx = { progress, user, floppy_total_bytes(g), 0,
                          g->cylinders, false };
    int rc = run_streaming(argv, gw_progress_line, &ctx);

    if (ctx.cancelled || rc == -2) { st = FLOPPY_ERR_CANCELLED; unlink(tmp); goto done; }
    if (rc != 0)                   { st = FLOPPY_ERR_IO;        unlink(tmp); goto done; }

    /* cargar el .img resultante */
    size_t len = 0; int ist = 0;
    uint8_t *img = image_load(tmp, &len, &ist);
    unlink(tmp);
    if (!img) { st = FLOPPY_ERR_IO; goto done; }

    /* ajustar al tamaño de la geometría (gw debería producir el tamaño exacto,
     * pero por robustez lo normalizamos) */
    size_t want = floppy_total_bytes(g);
    if (len != want) {
        uint8_t *fixed = calloc(want, 1);
        if (!fixed) { free(img); st = FLOPPY_ERR_NOMEM; goto done; }
        memcpy(fixed, img, len < want ? len : want);
        free(img);
        img = fixed;
    }
    result = img;
    st = FLOPPY_OK;

done:
    if (status) *status = st;
    return result;
}

int gw_write_image(const FloppyGeometry *g, const uint8_t *data, size_t len,
                   FloppyProgressFn progress, void *user) {
    if (!g || !data) return FLOPPY_ERR_BAD_PARAM;
    if (!gw_available()) return FLOPPY_ERR_OPEN;

    const char *fmt = gw_format_for(g);
    if (!fmt) return FLOPPY_ERR_BAD_PARAM;
    if (len != floppy_total_bytes(g)) return FLOPPY_ERR_SIZE;

    char tmp[600];
    if (!make_temp_img(tmp, sizeof tmp)) return FLOPPY_ERR_IO;

    /* volcar la imagen al archivo temporal */
    int ist = image_save(tmp, data, len);
    if (ist != IMG_OK) { unlink(tmp); return FLOPPY_ERR_IO; }

    /* gw write --format=<fmt> <tmp> */
    char fmtarg[64];
    snprintf(fmtarg, sizeof fmtarg, "--format=%s", fmt);
    char *argv[] = { "gw", "write", fmtarg, tmp, NULL };

    GwProgressCtx ctx = { progress, user, floppy_total_bytes(g), 0,
                          g->cylinders, false };
    int rc = run_streaming(argv, gw_progress_line, &ctx);
    unlink(tmp);

    if (ctx.cancelled || rc == -2) return FLOPPY_ERR_CANCELLED;
    if (rc != 0) return FLOPPY_ERR_IO;
    return FLOPPY_OK;
}
