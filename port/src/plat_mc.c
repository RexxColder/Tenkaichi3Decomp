/*
 * Memory card library (Sony libmc) on PC. For now: both slots are empty. Every request is accepted and finishes at
 * once with "no card" (result -10), which the game handles as a console without memory cards.
 * To do: a save folder (saves and replays as ordinary files) behind the same calls.
 * The names are the library's addresses in the PS2 executable; the game sources #define the sce names onto them.
 */
#include <stddef.h>

static int sLastCmd;

static int request(int cmd) {
    sLastCmd = cmd;
    return 0;
}

int func_002A1A48(void) { return 0; }                                           /* sceMcInit */
int func_002A1E58(int port, int slot, char *name, int mode) { return request(2); } /* sceMcOpen */
int func_002A1F80(int port, int slot, char *name) { return request(11); }       /* sceMcMkdir */
int func_002A1FB8(int fd) { return request(3); }                                /* sceMcClose */
int func_002A2078(int fd, int offset, int origin) { return request(4); }        /* sceMcSeek */
int func_002A2208(int fd, void *buf, int size) { return request(5); }           /* sceMcRead */
int func_002A2320(int fd, void *buf, int size) { return request(6); }           /* sceMcWrite */
int func_002A27A8(int port, int slot, char *name, int mode, int maxent, void *table) { return request(13); } /* sceMcGetDir */
int func_002A2AC8(int port, int slot) { return request(16); }                   /* sceMcFormat */

/* sceMcGetInfo: the outputs are filled when the request completes; "no card" leaves type 0. */
int func_002A25B8(int port, int slot, int *type, int *free, int *format) {
    if (type != NULL) { *type = 0; }
    if (free != NULL) { *free = 0; }
    if (format != NULL) { *format = 0; }
    return request(1);
}

/* sceMcSync: 1 = the request has finished (its number and result are returned), -1 = nothing was pending. */
int func_002A2498(int mode, int *cmd, int *result) {
    if (sLastCmd == 0) {
        return -1;
    }
    if (cmd != NULL) { *cmd = sLastCmd; }
    if (result != NULL) { *result = -10; }
    sLastCmd = 0;
    return 1;
}
