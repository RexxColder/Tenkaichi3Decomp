/*
 * Headless harness: stands in for the menu overlay. The game's own main loop is
 *     for (;;) { Overlay_Load(0); Progress_Main(0); Battle_Main(0); }
 * and Progress_Main is the menu's entry point: it returns when a battle has been set up. Here it sets one up
 * without any menu:
 *     BT3_REPLAY=<file>   a replay save file (0x1AC00 bytes: 0x38 header + the replay block)
 *     otherwise           the game's own attract-demo battle (CPU against CPU, one of nine fixed pairings)
 * and the second time it is called (the battle is over) it ends the program.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Battle_ClearWork(void);
extern int BattleReplay_Load(void *buf, int size);
extern void Demo_SetupBattle(void);

#define REPLAY_FILE_SIZE 0x1AC00
#define REPLAY_HEADER 0x38
#define REPLAY_BLOCK 0x1ABA8

static int sBattles;

void Progress_Main(int arg) {
    const char *path = getenv("BT3_REPLAY");

    (void)arg;
    if (sBattles++ > 0) {
        printf("bt3: battle finished\n");
        exit(0);
    }
    if (path != NULL) {
        static uint8_t buf[REPLAY_FILE_SIZE] __attribute__((aligned(16)));
        FILE *fp = fopen(path, "rb");

        if (fp == NULL || fread(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
            fprintf(stderr, "bt3: cannot read replay %s (0x%X bytes expected)\n", path, REPLAY_FILE_SIZE);
            exit(2);
        }
        fclose(fp);
        Battle_ClearWork();
        if (!BattleReplay_Load(buf + REPLAY_HEADER, REPLAY_BLOCK)) {
            fprintf(stderr, "bt3: %s is not a replay this version accepts\n", path);
            exit(2);
        }
        printf("bt3: replay %s\n", path);
    } else {
        Demo_SetupBattle();
        printf("bt3: attract-demo battle\n");
    }
    fflush(stdout);
}

/* Overlay_Load reads the menu overlay to its PS2 address: there is nothing to read on PC (size 0). */
int sceOpen(char *name, int flags) { (void)name; (void)flags; return 3; }
int sceLseek(int fd, int offset, int whence) { (void)fd; (void)offset; (void)whence; return 0; }
int sceRead(int fd, void *buf, int size) { (void)fd; (void)buf; return size; }
int sceClose(int fd) { (void)fd; return 0; }

/*
 * BT3_TRACE=<n>: every n vertical blanks, print the state of the fight (sequence state, battle clock words, health
 * and position of the two fighters). Read-only; called from the vertical blank (plat_stub.c).
 */
extern struct { int state; } *gBtlSeq;
extern int *BtlSeq_GetClock(void);
extern int BtlCharApi_GetHp(int objId);
extern void BtlCharApi_GetPos(int objId, float *out);

void Port_Trace(unsigned vblanks) {
    static int every = -1;
    float p[2][4] __attribute__((aligned(16)));
    unsigned u[2][3];
    int *clock;
    int i;

    if (every < 0) {
        every = getenv("BT3_TRACE") != NULL ? atoi(getenv("BT3_TRACE")) : 0;
    }
    /* BT3_AT=<tick>[,<tick>...]: when the battle clock first shows one of these tick counts, print the fight state
       and write the heap to port/build/heap_<tick>.bin (to compare with console save states made at those ticks). */
    if (sBattles != 0 && gBtlSeq != NULL && gBtlSeq->state >= 2 && getenv("BT3_AT") != NULL) {
        static unsigned last = 0xFFFFFFFFu;
        unsigned now = (unsigned)BtlSeq_GetClock()[0];

        if (now != last) {
            const char *t = getenv("BT3_AT");

            last = now;
            while (*t != '\0') {
                if ((unsigned)strtoul(t, (char **)&t, 0) == now) {
                    char name[64];
                    FILE *fp;

                    snprintf(name, sizeof(name), "port/build/heap_%u.bin", now);
                    fp = fopen(name, "wb");
                    fwrite((void *)0x3BE730, 1, 0x1EFB014 - 0x3BE730, fp);
                    fclose(fp);
                    {   /* the game's global variables too: [__data_start, _end), base address first */
                        extern char __data_start[], _end[];
                        uint32_t base = (uint32_t)(uintptr_t)__data_start;

                        snprintf(name, sizeof(name), "port/build/glob_%u.bin", now);
                        fp = fopen(name, "wb");
                        fwrite(&base, 4, 1, fp);
                        fwrite(__data_start, 1, (size_t)(_end - __data_start), fp);
                        fclose(fp);
                        snprintf(name, sizeof(name), "port/build/heap_%u.bin", now);
                    }
                    printf("bt3: tick %u (vblank %u, seq %d): hp %d %d; %s\n", now, vblanks, gBtlSeq->state,
                           BtlCharApi_GetHp(0), BtlCharApi_GetHp(1), name);
                    fflush(stdout);
                }
                if (*t == ',') {
                    t++;
                }
            }
        }
    }
    /* BT3_DUMP=<file>: 600 vertical blanks after the battle sequence reaches its last state, write the game heap
       (PS2 addresses 0x3BE730..0x1EFB014) to the file and stop: to be compared with a console memory dump. */
    if (sBattles != 0 && gBtlSeq != NULL && gBtlSeq->state == 6 && getenv("BT3_DUMP") != NULL) {
        static unsigned since;

        if (since == 0) {
            since = vblanks;
        } else if (vblanks - since == 600) {
            FILE *fp = fopen(getenv("BT3_DUMP"), "wb");

            fwrite((void *)0x3BE730, 1, 0x1EFB014 - 0x3BE730, fp);
            fclose(fp);
            clock = BtlSeq_GetClock();
            printf("bt3: end of battle: gBtlSeq %p clock %08x %08x %08x %08x hp %d %d; heap dumped\n", (void *)gBtlSeq,
                   clock[0], clock[1], clock[2], clock[3], BtlCharApi_GetHp(0), BtlCharApi_GetHp(1));
            exit(0);
        }
    }
    if (sBattles != 0 && gBtlSeq != NULL && getenv("BT3_TRACE_SEQ") != NULL) { /* when the battle sequence changes state */
        static int last = -1;
        if (gBtlSeq->state != last) {
            printf("seq: state %d begins at vertical blank %u\n", gBtlSeq->state, vblanks);
            fflush(stdout);
            last = gBtlSeq->state;
        }
    }
    if (every <= 0 || vblanks % (unsigned)every != 0 || sBattles == 0 || gBtlSeq == NULL || gBtlSeq->state < 2) {
        return;
    }
    clock = BtlSeq_GetClock();
    for (i = 0; i < 2; i++) {
        BtlCharApi_GetPos(i, p[i]);
        memcpy(u[i], p[i], 12);
    }
    /* BT3_CAMCHECK=<vblank>: test the pad fighter's line of sight (camera eye -> camera target) against EVERY
       triangle of the stage collision mesh, and say what the game's own sweep answers for the same segment. */
    if (getenv("BT3_CAMCHECK") != NULL && vblanks == (unsigned)atoi(getenv("BT3_CAMCHECK"))) {
        extern void *BtlChar_FindByObjId(int objId);
        extern struct { int nodeCount, unk4, unk8, nodeOfs, polyOfs, vtxOfs; } *gStgColMesh;
        extern void *ColMesh_GetPoly(void *mesh, int idx);
        extern void ColMesh_GetPolyVerts(void *mesh, void *poly, float *v0, float *v1, float *v2);
        extern int BtlCam_TraceStage(float *out, float *from, float *to, float *frac, int *hitObj);
        char *chr = BtlChar_FindByObjId(0);
        float *eye = (float *)(chr + 0x420), *tgt = (float *)(chr + 0x450);
        float v0[4] __attribute__((aligned(16))), v1[4] __attribute__((aligned(16))), v2[4] __attribute__((aligned(16)));
        float out[4] __attribute__((aligned(16))), frac = -1.0f;
        int n = gStgColMesh->unk4, hits = 0, obj = -1, k, r; /* unk4: taken as the polygon count */
        double d[3] = {tgt[0] - eye[0], tgt[1] - eye[1], tgt[2] - eye[2]};

        printf("camcheck: vblank %u, eye (%.2f %.2f %.2f) target (%.2f %.2f %.2f), %d polygons (counts in the header: %d, %d)\n", vblanks,
               (double)eye[0], (double)eye[1], (double)eye[2], (double)tgt[0], (double)tgt[1], (double)tgt[2], n, gStgColMesh->unk4,
               gStgColMesh->unk8);
        for (k = 0; k < n; k++) {
            double e1[3], e2[3], pv[3], tv[3], qv[3], det, u, v, t;
            int j;
            ColMesh_GetPolyVerts(gStgColMesh, ColMesh_GetPoly(gStgColMesh, k), v0, v1, v2);
            for (j = 0; j < 3; j++) { e1[j] = v1[j] - v0[j]; e2[j] = v2[j] - v0[j]; tv[j] = eye[j] - v0[j]; }
            pv[0] = d[1] * e2[2] - d[2] * e2[1]; pv[1] = d[2] * e2[0] - d[0] * e2[2]; pv[2] = d[0] * e2[1] - d[1] * e2[0];
            det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
            if (det > -1e-9 && det < 1e-9) { continue; }
            u = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) / det;
            qv[0] = tv[1] * e1[2] - tv[2] * e1[1]; qv[1] = tv[2] * e1[0] - tv[0] * e1[2]; qv[2] = tv[0] * e1[1] - tv[1] * e1[0];
            v = (d[0] * qv[0] + d[1] * qv[1] + d[2] * qv[2]) / det;
            t = (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2]) / det;
            if (u >= 0 && v >= 0 && u + v <= 1 && t >= 0 && t <= 1) {
                unsigned flags;
                memcpy(&flags, ColMesh_GetPoly(gStgColMesh, k), 4);
                if (hits++ < 6) {
                    printf("camcheck:   polygon %d crosses the line of sight at %.3f of the way (first word %08x)\n", k, t, flags);
                }
            }
        }
        r = BtlCam_TraceStage(out, eye, tgt, &frac, &obj);
        printf("camcheck: %d polygons cross the line of sight; the game's sweep (eye -> target) returns %d, fraction %.3f, zone %d\n", hits, r,
               (double)frac, obj);
        r = BtlCam_TraceStage(out, tgt, eye, &frac, &obj);
        printf("camcheck: the game's sweep the other way (target -> eye) returns %d, fraction %.3f, zone %d\n", r, (double)frac, obj);
        fflush(stdout);
    }
    {   /* the pad fighter's camera: eye position (fighter block + 0x420) against the ground height under the fighter */
        extern float BtlCharApi_GetGroundY(int objId);
        extern void *BtlChar_FindByObjId(int objId);
        float *chr = BtlChar_FindByObjId(0);
        if (chr != NULL && getenv("BT3_TRACE_CAM") != NULL) {
            float *eye = (float *)((char *)chr + 0x420), g = BtlCharApi_GetGroundY(0);
            unsigned ue[3], ug;
            memcpy(ue, eye, 12);
            memcpy(&ug, &g, 4);
            printf("cam tick %u eye %08x %08x %08x ground %08x\n", (unsigned)clock[0], ue[0], ue[1], ue[2], ug);
        }
    }
    printf("vb %7u seq %d clock %08x %08x %08x %08x | hp %6d %6d | p0 %08x %08x %08x | p1 %08x %08x %08x\n", vblanks,
           gBtlSeq->state, clock[0], clock[1], clock[2], clock[3], BtlCharApi_GetHp(0), BtlCharApi_GetHp(1),
           u[0][0], u[0][1], u[0][2], u[1][0], u[1][1], u[1][2]);
    fflush(stdout);
}
