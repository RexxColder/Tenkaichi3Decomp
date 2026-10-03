#ifndef BATTLE_BATTLE_H
#define BATTLE_BATTLE_H

#include "types.h"

/*
 * Battle scene top level: src/battle/battle.c = 0x12B570..0x12BD58 (eight functions).
 *
 *   Battle_Main      work->running = 1; Battle_Init(); Battle_Loop(); Battle_Term(); work->running = 0.
 *   Battle_Init      allocates every battle subsystem once, then Battle_Restart().
 *   Battle_Restart   puts every subsystem back to "start of match" without reloading; also run by
 *                    Battle_Loop when BATTLE_FLAG_RESTART is raised (rematch).
 *   Battle_Loop      one iteration per displayed frame, see below.
 *   Battle_Term      frees everything in roughly the reverse order.
 *
 * One frame (Battle_Loop), in this order:
 *    1. restart      if (flags & BATTLE_FLAG_RESTART) { Battle_Restart(); clear the flag; }
 *    2. Job_Run()    background loader jobs (character/stage streaming pushed by step 9)
 *    3. Gfx_BeginFrame()
 *    4. unless BATTLE_FLAG_PAUSE: func_00257A50 (walks the list at 0x333B80) and func_00259030
 *       (only acts in sequence states 3 and 5)
 *    5. func_001C2AA8()   fighter manager: first-frame hook when the sequence reaches state 2 or 3
 *    6. Pad_Update()      <- controller input is sampled here, once per frame
 *    7. Snd_Update(), func_00125330() (per-fighter sound update)
 *    8. BtlGame_PreUpdate()   HUD pre-update + BtlSeq_PreUpdate (sequence state's preUpdate)
 *    9. Battle_UpdateWork()   play-time counter and event bit sets (skipped while paused)
 *   10. func_001BB620(), func_001C2A28()   two more fighter-side passes (see battle.c)
 *   11. Battle_Update()       the simulation: fighters, objects/effects, cameras, visibility lists
 *   12. BtlGame_Update()      BtlSeq_Update: runs the state's update, switches state; returns 1 = leave
 *   13. draw                  Battle_DrawSplit() when split-screen and Battle_Update() returned 1,
 *                             else Battle_Draw()
 *   14. Gfx_EndFrame(2)       waits for 2 vsync ticks
 *   15. Dma_Flush()           sends the display list built by step 13
 *   The loop ends after the frame in which step 12 returned non-zero.
 *
 * Pause / restart / exit:
 *   - Pause is BATTLE_FLAG_PAUSE in work->flags. Nothing in this file tests it except step 4; every
 *     subsystem update tests it itself and returns early (126 readers), so a paused frame still runs
 *     the whole loop, reads the pad, runs the sequence and draws. The pause menu sets
 *     PAUSE | PAUSE_MENU (0x4100) from the sequence/HUD code (0x218230, 0x22FA80) and clears it at 0x218298.
 *   - Restart: BtlSeq_Update, on reaching state 99 with a rematch asked for, sets BATTLE_FLAG_RESTART
 *     and returns 0; the next iteration calls Battle_Restart() before anything else.
 *   - Exit: BtlSeq_Update returns 1 on state 99 otherwise; Battle_Loop returns, Battle_Main runs
 *     Battle_Term and returns 1 to Game_Main, which reloads the menu overlay.
 *
 * Split-screen (work->screenMode == 1): Battle_Update updates both player cameras in either case; it then
 * builds the object visibility lists once per view (2 views) instead of once. Battle_DrawSplit is
 * Battle_Draw with the two scene passes (stage, then fighters/objects) run once per view after
 * BtlCam_SelectView(i) + BtlCam_ApplyView(1) (which loads the view's matrices and scissor); the
 * effect, HUD and overlay passes are drawn once for the whole screen. When the camera module forces
 * a single full-screen view (BtlCam_UpdateOverride() != 0: cut-scene style cameras), Battle_Update
 * returns 0 and the frame is drawn by Battle_Draw even in split-screen.
 */

/* BattleWork.flags (64-bit). Bits not listed were not seen. */
#define BATTLE_FLAG_PAUSE       0x100  /* simulation frozen; tested by nearly every update function */
#define BATTLE_FLAG_UNK200      0x200  /* set/cleared by sequence states and by 0x12C880 / 0x12C8B0 */
#define BATTLE_FLAG_UNK400      0x400  /* set by sequence code at 0x218370 */
#define BATTLE_FLAG_UNK800      0x800  /* set while the loader job 0x1278B0 runs */
#define BATTLE_FLAG_UNK1000     0x1000 /* set while the loader job 0x127680 runs */
#define BATTLE_FLAG_LOADING     0x2000 /* set while the loader jobs 0x1272B0 / 0x127430 run: fighters, stage and
                                          cameras are neither updated nor drawn */
#define BATTLE_FLAG_PAUSE_MENU  0x4000 /* pause menu open (always set together with PAUSE) */
#define BATTLE_FLAG_RESTART     0x8000 /* Battle_Loop must call Battle_Restart() */
/* Bit 58 (0x8000 << 43) is tested by the effect draw pass 0x247578. */

/* One team member of a side (the unit Battle_GetSetup()->sides[s].members[m]). */
typedef struct BattleMember {
    /* 0x00 */ s32 unk0;      /* returned by 0x12B058 */
    /* 0x04 */ s32 unk4;      /* returned by 0x12B0C0 */
    /* 0x08 */ s32 unk8;      /* returned by 0x12B128 */
    /* 0x0C */ s32 unkC[2];
    /* 0x14 */ u8 unk14[0x10]; /* 16 bytes copied in by 0x12B490 */
    /* 0x24 */ u8 unk24[0x40];
} BattleMember; /* size 0x64 */

/* One side (player) of the battle. */
typedef struct BattleSide {
    /* 0x000 */ s32 unk0;                 /* returned by 0x12B190 */
    /* 0x004 */ BattleMember members[5];
    /* 0x1F8 */ s32 unk1F8;
    /* 0x1FC */ s32 unk1FC;               /* returned by 0x12AE58 */
    /* 0x200 */ s32 unk200;               /* returned by 0x12AE18 */
    /* 0x204 */ s32 unk204;               /* returned by 0x12AD50 */
    /* 0x208 */ s32 unk208;               /* returned by 0x12AD90; 0 = side unused (0x12AA70), 2 tested by 0x12ADD0 */
    /* 0x20C */ s32 unk20C;
    /* 0x210 */ u64 unk210[8];            /* bit array, bit n tested by 0x12B4F0 (length not established) */
    /* 0x250 */ s32 unk250[3];            /* initial triple (0x12AE98, 0x12AED8, 0x12AF18) */
    /* 0x25C */ s32 unk25C[3];            /* current triple, compared against unk250 by 0x12B378 */
    /* 0x268 */ s32 objId;                /* argument for BtlObj_Get: the side's active fighter object (0x12B1D0) */
    /* 0x26C */ s32 unk26C;               /* 0x12B210 / 0x12B2A0 */
} BattleSide; /* size 0x270 */

/* Result block, cleared by Battle_ResetWork (memset 0x48), filled during the match and by 0x1291C0 at Term. */
typedef struct BattleResult {
    /* 0x00 */ s32 flags;      /* bits 0, 1, 3 tested by 0x128960..0x128A78 */
    /* 0x04 */ s32 reason;     /* bits 0x18000: restart requested (Battle_IsRematchRequested) */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ u64 unk10;
    /* 0x18 */ s32 frames;     /* +1 per unpaused frame while event bit 0x48 of set 0 is clear (0x129170) */
    /* 0x1C */ u8 unk1C[0x18];
    /* 0x34 */ u8 unk34[0x10]; /* 16 bytes copied from 0x216E00() by 0x1291C0 */
    /* 0x44 */ s32 unk44;
} BattleResult; /* size 0x48 */

/* The battle's shared state: a static block returned by Battle_GetWork() / Battle_GetSetup(). */
typedef struct BattleWork {
    /* 0x0000 */ s32 unk0;
    /* 0x0004 */ s32 unk4;
    /* 0x0008 */ s32 mode;        /* Battle_GetMode(): 1..8, picks the sequence table */
    /* 0x000C */ s32 bgm;         /* Battle_ResetWork plays BGM file 0x10B16 + bgm */
    /* 0x0010 */ s32 unk10;       /* index into the s16 table 0x2C3480 (0x12AB58); 0 tested by 0x12AB90 */
    /* 0x0014 */ s32 unk14;       /* 0x12ABB8 */
    /* 0x0018 */ s32 unk18;       /* 0x12ACD0 */
    /* 0x001C */ s32 unk1C;       /* 0x12ABF8; copied to unk28 by 0x12AC98 */
    /* 0x0020 */ s32 unk20;
    /* 0x0024 */ s32 screenMode;  /* 1 = split-screen (Battle_IsSplitScreen) */
    /* 0x0028 */ s32 unk28;       /* 0x12AC18 / 0x12AC38 */
    /* 0x002C */ s32 unk2C;
    /* 0x0030 */ s32 unk30[2];    /* per side, 0x12ACF0 */
    /* 0x0038 */ s32 unk38[2];    /* per side, 0x12AD20 */
    /* 0x0040 */ s32 unk40;       /* 0x12AAC8 */
    /* 0x0044 */ u8 unk44[0x78];
    /* 0x00BC */ s32 unkBC;       /* stored as value + 1 by 0x129F28, read back - 1 by 0x12AAE8 (loader job argument) */
    /* 0x00C0 */ BattleSide sides[2];
    /* 0x05A0 */ u8 unk5A0[8];
    /* 0x05A8 */ u8 unk5A8[0x1390]; /* sub-struct returned by 0x127048 */
    /* 0x1938 */ BattleResult result; /* Battle_GetResult() */
    /* 0x1980 */ u8 unk1980[0x70];  /* sub-struct returned by 0x127028: +0 kept across reset, +8 two 0x30-byte event
                                       bit sets {new[2], prev[2], held[2]} of u64, +0x68, +0x6C */
    /* 0x19F0 */ u64 flags;         /* BATTLE_FLAG_* */
    /* 0x19F8 */ s32 running;       /* 1 between Battle_Main's entry and exit */
    /* 0x19FC */ s32 unk19FC;
} BattleWork; /* size 0x1A00 */

BattleWork *Battle_GetWork(void);
BattleWork *Battle_GetSetup(void);
s32 Battle_IsSplitScreen(void);
s32 Battle_GetMode(void);

s32 Battle_Restart(void);
s32 Battle_Init(void);
s32 Battle_Term(void);
s32 Battle_Update(void);
s32 Battle_Draw(void);
s32 Battle_DrawSplit(void);
void Battle_Loop(void);
s32 Battle_Main(void);

#endif
