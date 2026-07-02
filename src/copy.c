/*
 * copy - pipe stdin to the system clipboard, auto-detecting backend.
 *
 * Works on Linux (X11, Wayland), WSL, and macOS.
 *
 * Usage:
 *   echo hello | copy
 *   cat file.log | copy
 *   tail -f app.log | copy -n 20
 *
 * Author: MaxEdgar (https://github.com/MaxEdgar)
 * License: MIT
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#define PROGRAM_NAME "copy"
#define VERSION "1.2.0"

#define INITIAL_BUF (1024 * 256)            /* start at 256KB, grow as needed */
#define MAX_BUF ((size_t)1024 * 1024 * 512) /* 512 MB safety ceiling, not a normal-use limit */

typedef struct {
    const char *bin;
    const char *args[4]; /* NULL-terminated argv after bin */
} backend_t;

typedef struct {
    long n_lines;   /* -n: keep only the last N lines, 0 = disabled */
    long n_chars;   /* -c: keep only the last N characters, 0 = disabled */
    int print;      /* -p / --print: also print what was copied to stdout */
    int strip_trailing_newline; /* default on; -k / --keep-newline disables */
} options_t;

static void print_usage(FILE *out) {
    fprintf(out,
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Pipe stdin to the system clipboard. Auto-detects X11, Wayland, WSL,\n"
        "and macOS clipboard backends.\n"
        "\n"
        "Options:\n"
        "  -n, --lines N       Only copy the last N lines of input (like tail -n)\n"
        "  -c, --chars N       Only copy the last N characters of input\n"
        "  -p, --print         Also print the copied text to stdout\n"
        "  -k, --keep-newline  Keep a trailing newline if present (default: stripped)\n"
        "  -h, --help          Show this help message and exit\n"
        "  -v, --version       Show version information and exit\n"
        "\n"
        "Examples:\n"
        "  echo hello | %s              Copy \"hello\" to the clipboard\n"
        "  cat notes.txt | %s           Copy the whole file\n"
        "  tail -f app.log | %s -n 20   Copy only the last 20 lines\n"
        "  dmesg | %s -c 500            Copy only the last 500 characters\n"
        "\n"
        "If no clipboard backend is installed, %s will report exactly what to\n"
        "install for your session (xclip, xsel, or wl-clipboard).\n"
        "\n"
        "Homepage: https://github.com/MaxEdgar/copy\n",
        PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME, PROGRAM_NAME);
}

static void print_version(FILE *out) {
    fprintf(out, "%s %s\n", PROGRAM_NAME, VERSION);
    fprintf(out, "Author: MaxEdgar (https://github.com/MaxEdgar)\n");
    fprintf(out, "License: MIT\n");
}

static int which(const char *bin) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", bin);
    return system(cmd) == 0;
}

static int is_wsl(void) {
    FILE *f = fopen("/proc/version", "r");
    if (!f) return 0;
    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    return strstr(buf, "microsoft") != NULL || strstr(buf, "wsl") != NULL;
}

/* Run bin with args, feeding `data` (len bytes) to its stdin.
 * Returns 1 on success (exit code 0), 0 otherwise. */
static int run_pipe(const char *bin, const char *const argv[], const char *data, size_t len) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return 0;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return 0;
    }

    if (pid == 0) {
        /* child */
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execvp(bin, (char *const *)argv);
        _exit(127);
    }

    /* parent */
    close(pipefd[0]);
    size_t written = 0;
    while (written < len) {
        ssize_t w = write(pipefd[1], data + written, len - written);
        if (w < 0) {
            if (errno == EINTR) continue; /* interrupted, just retry */
            break;                        /* EPIPE (child exited early) or other error: stop writing */
        }
        written += (size_t)w;
    }
    close(pipefd[1]);

    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* Read all of stdin into a dynamically growing buffer.
 * On success returns a malloc'd buffer via *out_buf and length via *out_len.
 * Caller must free *out_buf. Returns 0 on success, non-zero on failure
 * (in which case *out_buf is NULL and an error has already been printed). */
static int read_all_stdin(char **out_buf, size_t *out_len) {
    size_t cap = INITIAL_BUF;
    size_t total = 0;
    char *buf = malloc(cap);
    if (!buf) {
        fprintf(stderr, "%s: out of memory\n", PROGRAM_NAME);
        return 1;
    }

    for (;;) {
        if (total == cap) {
            if (cap >= MAX_BUF) {
                fprintf(stderr,
                        "%s: input exceeds %zuMB safety limit, refusing to copy "
                        "(nothing was copied). Use a file-based tool for input this large.\n",
                        PROGRAM_NAME, MAX_BUF / (1024 * 1024));
                free(buf);
                return 1;
            }
            size_t new_cap = cap * 2;
            if (new_cap > MAX_BUF) new_cap = MAX_BUF;
            char *nbuf = realloc(buf, new_cap);
            if (!nbuf) {
                fprintf(stderr, "%s: out of memory while growing buffer (nothing was copied)\n", PROGRAM_NAME);
                free(buf);
                return 1;
            }
            buf = nbuf;
            cap = new_cap;
        }

        ssize_t r = read(STDIN_FILENO, buf + total, cap - total);
        if (r < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "%s: error reading stdin: %s\n", PROGRAM_NAME, strerror(errno));
            free(buf);
            return 1;
        }
        if (r == 0) break; /* EOF */
        total += (size_t)r;
    }

    *out_buf = buf;
    *out_len = total;
    return 0;
}

/* Trim input down to the last n_lines lines (like `tail -n`), in place.
 * Returns a pointer into buf where the kept region starts, and sets
 * *out_len to its length. Does not reallocate. */
static const char *trim_to_last_lines(const char *buf, size_t len, long n_lines, size_t *out_len) {
    if (n_lines <= 0 || len == 0) {
        *out_len = len;
        return buf;
    }

    size_t i;
    int lines_seen = 0;

    /* Skip a single trailing newline, if present, so it doesn't get
     * counted as an extra blank line boundary. */
    size_t scan_end = len;
    if (scan_end > 0 && buf[scan_end - 1] == '\n') {
        scan_end--;
    }

    i = scan_end;
    while (i > 0) {
        i--;
        if (buf[i] == '\n') {
            lines_seen++;
            if (lines_seen == n_lines) {
                *out_len = len - (i + 1);
                return buf + i + 1;
            }
        }
    }

    /* Fewer than n_lines newlines found: the whole input is <= n_lines lines. */
    *out_len = len;
    return buf;
}

/* Trim input down to the last n_chars characters, in place (no realloc). */
static const char *trim_to_last_chars(const char *buf, size_t len, long n_chars, size_t *out_len) {
    if (n_chars <= 0 || (size_t)n_chars >= len) {
        *out_len = len;
        return buf;
    }
    *out_len = (size_t)n_chars;
    return buf + (len - (size_t)n_chars);
}

static int parse_long_arg(const char *arg, const char *flag_name, long *out) {
    if (!arg || *arg == '\0') {
        fprintf(stderr, "%s: %s requires a numeric argument\n", PROGRAM_NAME, flag_name);
        return 1;
    }
    char *end = NULL;
    long v = strtol(arg, &end, 10);
    if (end == arg || *end != '\0' || v < 0) {
        fprintf(stderr, "%s: invalid value '%s' for %s (expected a non-negative integer)\n",
                PROGRAM_NAME, arg, flag_name);
        return 1;
    }
    *out = v;
    return 0;
}

int main(int argc, char **argv) {
    options_t opt;
    memset(&opt, 0, sizeof(opt));
    opt.strip_trailing_newline = 1;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(stdout);
            return 0;
        }
        if (strcmp(a, "-v") == 0 || strcmp(a, "--version") == 0) {
            print_version(stdout);
            return 0;
        }
        if (strcmp(a, "-p") == 0 || strcmp(a, "--print") == 0) {
            opt.print = 1;
            continue;
        }
        if (strcmp(a, "-k") == 0 || strcmp(a, "--keep-newline") == 0) {
            opt.strip_trailing_newline = 0;
            continue;
        }
        if (strcmp(a, "-n") == 0 || strcmp(a, "--lines") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: -n/--lines requires a numeric argument\n", PROGRAM_NAME);
                return 1;
            }
            if (parse_long_arg(argv[++i], "-n/--lines", &opt.n_lines)) return 1;
            continue;
        }
        if (strncmp(a, "--lines=", 8) == 0) {
            if (parse_long_arg(a + 8, "--lines", &opt.n_lines)) return 1;
            continue;
        }
        if (strcmp(a, "-c") == 0 || strcmp(a, "--chars") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: -c/--chars requires a numeric argument\n", PROGRAM_NAME);
                return 1;
            }
            if (parse_long_arg(argv[++i], "-c/--chars", &opt.n_chars)) return 1;
            continue;
        }
        if (strncmp(a, "--chars=", 8) == 0) {
            if (parse_long_arg(a + 8, "--chars", &opt.n_chars)) return 1;
            continue;
        }

        fprintf(stderr, "%s: unrecognized option '%s'\n", PROGRAM_NAME, a);
        fprintf(stderr, "Try '%s --help' for usage information.\n", PROGRAM_NAME);
        return 1;
    }

    if (opt.n_lines > 0 && opt.n_chars > 0) {
        fprintf(stderr, "%s: -n/--lines and -c/--chars cannot be used together\n", PROGRAM_NAME);
        return 1;
    }

    /* Without this, if the clipboard tool exits early while we're still
     * writing to it, the kernel sends SIGPIPE and kills us outright instead
     * of letting write() return EPIPE, which we already handle gracefully. */
    signal(SIGPIPE, SIG_IGN);

    char *buf = NULL;
    size_t total = 0;
    if (read_all_stdin(&buf, &total) != 0) {
        return 1;
    }

    if (total == 0) {
        fprintf(stderr, "%s: no input on stdin (usage: echo hello | %s)\n", PROGRAM_NAME, PROGRAM_NAME);
        free(buf);
        return 1;
    }

    const char *payload = buf;
    size_t payload_len = total;

    if (opt.n_lines > 0) {
        payload = trim_to_last_lines(buf, total, opt.n_lines, &payload_len);
    } else if (opt.n_chars > 0) {
        payload = trim_to_last_chars(buf, total, opt.n_chars, &payload_len);
    }

    if (opt.strip_trailing_newline && payload_len > 0 && payload[payload_len - 1] == '\n') {
        payload_len--;
    }

    if (payload_len == 0) {
        fprintf(stderr, "%s: nothing left to copy after filtering (empty result)\n", PROGRAM_NAME);
        free(buf);
        return 1;
    }

    const char *session = getenv("XDG_SESSION_TYPE");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    const char *x_display = getenv("DISPLAY");

    backend_t candidates[8];
    int nc = 0;

    if (is_wsl() && which("clip.exe")) {
        candidates[nc].bin = "clip.exe";
        candidates[nc].args[0] = "clip.exe";
        candidates[nc].args[1] = NULL;
        nc++;
    }

    int wayland_session = (session && strcmp(session, "wayland") == 0) || wayland_display;
    if (wayland_session && which("wl-copy")) {
        candidates[nc].bin = "wl-copy";
        candidates[nc].args[0] = "wl-copy";
        candidates[nc].args[1] = NULL;
        nc++;
    }

    int x11_session = (session && strcmp(session, "x11") == 0) || x_display;
    if (x11_session && which("xclip")) {
        candidates[nc].bin = "xclip";
        candidates[nc].args[0] = "xclip";
        candidates[nc].args[1] = "-selection";
        candidates[nc].args[2] = "clipboard";
        candidates[nc].args[3] = NULL;
        nc++;
    }
    if (x11_session && which("xsel")) {
        candidates[nc].bin = "xsel";
        candidates[nc].args[0] = "xsel";
        candidates[nc].args[1] = "--clipboard";
        candidates[nc].args[2] = "--input";
        candidates[nc].args[3] = NULL;
        nc++;
    }

    if (which("pbcopy")) {
        candidates[nc].bin = "pbcopy";
        candidates[nc].args[0] = "pbcopy";
        candidates[nc].args[1] = NULL;
        nc++;
    }

    /* Fallback: env vars weren't conclusive (e.g. run from cron/su/script) -
     * just try everything installed. */
    if (nc == 0) {
        struct { const char *bin; const char *a1, *a2; } fb[] = {
            {"wl-copy", NULL, NULL},
            {"xclip", "-selection", "clipboard"},
            {"xsel", "--clipboard", "--input"},
            {"clip.exe", NULL, NULL},
            {"pbcopy", NULL, NULL},
        };
        for (size_t i = 0; i < sizeof(fb) / sizeof(fb[0]); i++) {
            if (which(fb[i].bin)) {
                candidates[nc].bin = fb[i].bin;
                int k = 0;
                candidates[nc].args[k++] = fb[i].bin;
                if (fb[i].a1) candidates[nc].args[k++] = fb[i].a1;
                if (fb[i].a2) candidates[nc].args[k++] = fb[i].a2;
                candidates[nc].args[k] = NULL;
                nc++;
            }
        }
    }

    if (nc == 0) {
        fprintf(stderr, "%s: no clipboard backend found.\n", PROGRAM_NAME);
        fprintf(stderr, "Install one depending on your session:\n");
        fprintf(stderr, "  Wayland : sudo apt install wl-clipboard   (or pacman -S wl-clipboard)\n");
        fprintf(stderr, "  X11     : sudo apt install xclip           (or pacman -S xclip)\n");
        free(buf);
        return 1;
    }

    int copied = 0;
    for (int i = 0; i < nc; i++) {
        if (run_pipe(candidates[i].bin, candidates[i].args, payload, payload_len)) {
            copied = 1;
            break;
        }
    }

    if (!copied) {
        fprintf(stderr, "%s: found a clipboard backend but failed to copy. Is a display/session active?\n", PROGRAM_NAME);
        free(buf);
        return 1;
    }

    if (opt.print) {
        fwrite(payload, 1, payload_len, stdout);
        if (payload_len > 0 && payload[payload_len - 1] != '\n') {
            fputc('\n', stdout);
        }
    }

    free(buf);
    return 0;
}
