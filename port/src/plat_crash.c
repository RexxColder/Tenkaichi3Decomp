/*
 * Crash report for the PC build: on a fatal signal, writes the signal and a backtrace (function names; the build
 * is linked without PIE and with its symbols) to the terminal and to bt3_crash.txt in the current directory, then
 * lets the default action happen. So a crash someone else hits can be diagnosed from one file.
 */
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

static void put(int fd, const char *s) {
    if (write(fd, s, strlen(s)) < 0) {
    }
}

static void on_crash(int sig) {
    void *frames[48];
    int n = backtrace(frames, 48), fd = open("bt3_crash.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644), k;
    const char *name = sig == SIGSEGV ? "SIGSEGV (bad memory access)" : sig == SIGFPE ? "SIGFPE (arithmetic)" :
                       sig == SIGILL ? "SIGILL (bad instruction)" : sig == SIGBUS ? "SIGBUS" : "SIGABRT";

    for (k = 0; k < 2; k++) {
        int out = k == 0 ? 2 : fd;
        if (out < 0) {
            continue;
        }
        put(out, "bt3: crashed: ");
        put(out, name);
        put(out, "\nbacktrace (innermost first):\n");
        backtrace_symbols_fd(frames, n, out);
    }
    if (fd >= 0) {
        close(fd);
        put(2, "bt3: the same report is in bt3_crash.txt\n");
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

__attribute__((constructor)) static void crash_init(void) {
    static const int sigs[] = {SIGSEGV, SIGFPE, SIGILL, SIGBUS, SIGABRT};
    unsigned i;

    for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) {
        signal(sigs[i], on_crash);
    }
}
