/*
 * Headless stand-ins: sound, the GS, pads, movies. Everything here does nothing and reports "idle / done", which
 * is what the first milestone (the fight simulation without picture or sound) needs. Each group gets a real
 * implementation later and then moves to its own file. Arguments are ignored (the callers clean the stack).
 */
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <time.h>

/* ---- second processor (IOP): heap and remote calls ---- */
int sceSifInitIopHeap() { return 0; }
void *sceSifAllocIopHeap(int size) { return calloc(1, (size_t)size); } /* "IOP memory": ordinary memory */
int sceSifFreeIopHeap(void *addr) { free(addr); return 0; }
int sceSifQueryTotalFreeMemSize() { return 0x100000; }
int sceSifQueryMaxFreeMemSize() { return 0x100000; }
/* The bind is answered at once: sceSifClientData.serve (offset 0x24) becomes non-NULL. */
int sceSifBindRpc(void *client, int id, int mode) { (void)id; (void)mode; *(void **)((uint8_t *)client + 0x24) = client; return 0; }
int func_002B4AF0() { return 0; }      /* sceSifCheckStatRpc: never busy */
/* sceSifCallRpc, sceSifSetDma, sceSdRemote: the sound effects, port/src/gs/snd_se.c */
int sceSifDmaStat() { return -1; }     /* transfer finished */

/* ---- sound driver ---- */
int sceSdRemoteInit() { return 0; }
int func_00296B48() { return 0; }      /* sceSdRemoteCallbackInit */

/* ---- CRI ADXT stream players ---- */
/* ---- CRI ADXT (streamed music and voices): port/src/gs/snd_adx.c ---- */

/* ---- GS ---- */
static int (*sVsyncHandler)(int);
void sceGsResetGraph() {}
void sceGsResetPath() {}
void sceGsSetDefStoreImage() {}
int sceGsExecStoreImage() { return 0; }
int sceGsSyncPath() { return 0; }
void *sceGsSyncVCallback(int (*handler)(int)) { void *old = (void *)sVsyncHandler; sVsyncHandler = handler; return old; }
/* One vertical blank: runs the game's VBlank handler, as the interrupt would. */
extern void Port_Trace(unsigned vblanks);
unsigned gPortVBlanks; /* vertical blanks since start: the headless build's clock */
/* With a window, a vertical blank is also the clock: one every 1/59.94 s. The game waits for one per frame in
   the menus (60 frames per second) and two in a battle (30), as on the console. Headless runs do not wait. */
extern int GsGpu_Enabled(void);
unsigned long long gPortSleptNs; /* time spent waiting here (the renderer's frame timing subtracts it) */

static void vblank_wait(void) {
    static unsigned long long next;
    struct timespec ts;
    unsigned long long t;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    t = (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
    if (next > t && next - t < 100000000ull) {
        ts.tv_sec = 0;
        ts.tv_nsec = (long)(next - t);
        nanosleep(&ts, NULL);
        gPortSleptNs += next - t;
        next += 16683350ull;
    } else {
        next = t + 16683350ull; /* late (or the first one): start a new grid from now */
    }
}

void Port_VBlank(void) {
    /* BT3_PACED=1: real-time pacing without a window too (sound tests) */
    if ((GsGpu_Enabled() || getenv("BT3_PACED") != NULL) && getenv("BT3_UNCAPPED") == NULL) {
        vblank_wait();
    }
    gPortVBlanks++;
    Port_Trace(gPortVBlanks);
    if (sVsyncHandler != NULL) {
        sVsyncHandler(0);
    }
}
int sceGsSyncV() { Port_VBlank(); return 0; }

/* ---- pads ---- */
int sceDbcInit() { return 1; }
int scePad2Init() { return 1; }
static int sPadSockets;
int scePad2CreateSocket() { return sPadSockets++; }
int scePad2GetState() { return 1; }            /* connected and ready */
/* A DualShock 2's button profile: every digital button, both sticks, every pressure-sensitive button. */
int scePad2GetButtonProfile(int socket, unsigned char *profile) {
    (void)socket;
    profile[0] = 0xFF; profile[1] = 0xFF; profile[2] = 0xFF; profile[3] = 0x03;
    return 4;
}
/* With a window: keyboard and gamepads (port/src/gs/gs_input.c). Headless: nobody touches the pad. Buttons are
   active-low; the sticks rest at 0x80. */
extern int Port_PadRead(int socket, unsigned char *data);

int scePad2Read(int socket, unsigned char *data) {
    int i;

    if (Port_PadRead(socket, data)) {
        return 18;
    }
    data[0] = 0xFF; data[1] = 0xFF;
    for (i = 2; i < 6; i++) { data[i] = 0x80; }
    for (i = 6; i < 18; i++) { data[i] = 0; }
    return 18;
}
int sceVibGetProfile() { return 0; }
int sceVibSetActParam() { return 0; }

/* ---- MPEG movies: port/src/plat_movie.c ---- */

/* ---- widescreen ----
   BT3_WIDE=1 (or port/run.sh wide): a 16:9 picture. The game's projection gets 4/3 more width of view at the same
   height (View_SetProjection, src/battle/btl_cam.c); the renderer keeps 2D art at its own proportions and shows
   the picture at 16:9 (port/src/gs/gs_gpu.c). */
int gPortWide = -1;
int Port_IsWide(void) {
    if (gPortWide < 0) {
        if (getenv("BT3_WIDE") != NULL) {
            gPortWide = atoi(getenv("BT3_WIDE")) != 0;
        } else {
            /* not said: a window size wider than 3:2 (BT3_WINDOW=1920x1080) asks for widescreen by itself */
            int w = 0, h = 0;
            gPortWide = getenv("BT3_WINDOW") != NULL && sscanf(getenv("BT3_WINDOW"), "%dx%d", &w, &h) == 2 && h > 0 && w * 2 > h * 3;
        }
    }
    return gPortWide;
}
float Port_WideFactor(void) {
    return Port_IsWide() ? 4.0f / 3.0f : 1.0f;
}
