/*
 * Headless stand-ins: sound, the GS, pads, movies. Everything here does nothing and reports "idle / done", which
 * is what the first milestone (the fight simulation without picture or sound) needs. Each group gets a real
 * implementation later and then moves to its own file. Arguments are ignored (the callers clean the stack).
 */
#include <stdint.h>
#include <stdlib.h>

/* ---- second processor (IOP): heap and remote calls ---- */
void sceSifInitIopHeap() {}
void *sceSifAllocIopHeap(int size) { return calloc(1, (size_t)size); } /* "IOP memory": ordinary memory */
int sceSifFreeIopHeap(void *addr) { free(addr); return 0; }
int sceSifQueryTotalFreeMemSize() { return 0x100000; }
int sceSifQueryMaxFreeMemSize() { return 0x100000; }
/* The bind is answered at once: sceSifClientData.serve (offset 0x24) becomes non-NULL. */
int sceSifBindRpc(void *client, int id, int mode) { (void)id; (void)mode; *(void **)((uint8_t *)client + 0x24) = client; return 0; }
int sceSifCallRpc() { return 0; }      /* the reply buffer stays as the caller left it */
int func_002B4AF0() { return 0; }      /* sceSifCheckStatRpc: never busy */
uint32_t sceSifSetDma() { return 1; }
int sceSifDmaStat() { return -1; }     /* transfer finished */

/* ---- sound driver ---- */
int sceSdRemoteInit() { return 0; }
int func_00296B48() { return 0; }      /* sceSdRemoteCallbackInit */
/* sceSdRemote: the sample-upload status query (rSdVoiceTransStatus, 0x80F0) answers "finished". */
int func_002967C0(int block, int cmd) { (void)block; return cmd == 0x80F0 ? 1 : 0; }

/* ---- CRI ADXT stream players ---- */
static uint8_t sAdxt[8][0x100];
static int sAdxtCount;
void ADXT_Init() {}
void *ADXT_Create() { return sAdxt[sAdxtCount++ & 7]; }
void ADXT_SetReloadSct() {}
void ADXT_SetSvrFreq() {}
void ADXT_StartAfs() {}
void ADXT_StartFname() {}
void ADXT_Stop() {}
void ADXT_Pause() {}
void ADXT_SetOutVol() {}
void ADXT_SetOutPan() {}
void ADXT_SetOutputMono() {}
int ADXT_GetOutVol() { return 0; }
int ADXT_GetStat() { return 0; }       /* ADXT_STAT_STOP */

/* ---- GS ---- */
static int (*sVsyncHandler)(int);
void sceGsResetGraph() {}
void sceGsResetPath() {}
void sceGsSetDefDBuff() {}
void sceGsSetDefStoreImage() {}
void sceGsExecStoreImage() {}
void sceGsPutDrawEnv() {}
int sceGsSwapDBuff() { return 0; }
int sceGsSyncPath() { return 0; }
void *sceGsSyncVCallback(int (*handler)(int)) { void *old = (void *)sVsyncHandler; sVsyncHandler = handler; return old; }
/* One vertical blank: runs the game's VBlank handler, as the interrupt would. */
unsigned gPortVBlanks; /* vertical blanks since start: the headless build's clock */
void Port_VBlank(void) {
    gPortVBlanks++;
    if (sVsyncHandler != NULL) {
        sVsyncHandler(0);
    }
}
int sceGsSyncV() { Port_VBlank(); return 0; }

/* ---- pads ---- */
int sceDbcInit() { return 1; }
int scePad2Init() { return 1; }
int scePad2CreateSocket() { return 0; }
int scePad2GetState() { return 0; }
int scePad2GetButtonProfile() { return 0; }
int scePad2Read() { return 0; }
int sceVibGetProfile() { return 0; }
int sceVibSetActParam() { return 0; }

/* ---- MPEG movies: every movie is over at once ---- */
int sceMpegInit() { return 0; }
int sceMpegCreate() { return 0; }
int sceMpegDelete() { return 0; }
int sceMpegReset() { return 0; }
void *sceMpegAddCallback() { return 0; }
void *sceMpegAddStrCallback() { return 0; }
int sceMpegDemuxPss() { return 0; }
int sceMpegGetPicture() { return 0; }
int sceMpegIsEnd() { return 1; }
