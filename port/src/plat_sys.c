/*
 * Sony kernel, SIF and CD/DVD calls of the game on PC. There is no second processor, no modules to load and no
 * drive: these report success. (Files are served by plat_file.c.)
 */
#include <stdint.h>

int GetThreadId(void) { return 1; }
int ChangeThreadPriority(int tid, int prio) { (void)tid; (void)prio; return 0; }
void FlushCache(int mode) { (void)mode; }

void sceSifInitRpc(uint32_t mode) { (void)mode; }
int sceSifRebootIop(char *img) { (void)img; return 1; }
int sceSifSyncIop(void) { return 1; }
int sceSifLoadModule(char *name, int argLen, char *args) { (void)name; (void)argLen; (void)args; return 0; }
int sceFsReset(void) { return 0; }

int sceCdInit(int mode) { (void)mode; return 1; }
int sceCdMmode(int media) { (void)media; return 1; }

/* func_002BB390: the library call whose result seeds libc rand at start-up (Rand_Init; a clock or time value on
   the PS2, not identified). Fixed here so that a run is repeatable; BT3_SEED overrides it. */
#include <stdlib.h>
long func_002BB390(void) {
    const char *s = getenv("BT3_SEED");
    return s != NULL ? strtol(s, NULL, 0) : 0x12345678;
}
