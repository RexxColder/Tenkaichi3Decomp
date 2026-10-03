#ifndef BATTLE_BTL_AI_MGR_H
#define BATTLE_BTL_AI_MGR_H

#include "types.h"
#include "sys/math3d.h"

/*
 * CPU opponent ("AI") manager and its "move" action: src/battle/btl_ai_mgr.c, 0x1BB128..0x1BC8A8.
 * See the block comment at the top of the C file.
 *
 * include/battle/btl_ai.h (another module, 0x1B6B08..) describes the same heap block. This header is a
 * local view with its own type names (rule: no shared header for structs several agents touch); the two
 * have to be merged afterwards. Fields marked (n/m) are written or read only by AI code outside this file
 * and are here from a first read of that code.
 */

/* A vector as this file's locals hold it: Vec4 with the 8-byte (or more) alignment the original type had
   (the struct copies are ld/sd pairs). sys/math3d.h's Vec4 is 4-byte aligned and gives ldl/ldr. */
typedef struct BtlAiVec {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(8))) BtlAiVec; /* size 0x10 */

/* A path point. Same four floats as Vec4, but the original copied these with unaligned loads
   (ldl/ldr), so the type had 4-byte alignment, unlike the stack vectors. */
typedef struct BtlAiMovePoint {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} BtlAiMovePoint; /* size 0x10 */

/* Segment handed to the stage line test func_001B2DF0. */
typedef struct BtlAiSegment {
    /* 0x00 */ BtlAiVec from;
    /* 0x10 */ BtlAiVec to;
} BtlAiSegment; /* size 0x20 */

/* Result of the last stage line test (static block D_0031C0A0, func_001B2F40 returns it). */
typedef struct BtlAiHit {
    /* 0x00 */ s32 hit;          /* (n/m) 1 when the segment is blocked */
    /* 0x04 */ u8 unk04[0x0C];
    /* 0x10 */ BtlAiVec pos;         /* (n/m) */
    /* 0x20 */ u8 unk20[0x20];   /* (n/m) */
    /* 0x40 */ s32 id;           /* -1: what blocks the segment has no id (inferred: not a breakable object) */
} BtlAiHit;

/* One entry of the action stack. */
typedef struct BtlAiMgrActEntry {
    /* 0x00 */ s32 id;           /* action id: index of the u16 flag table at BtlAiMgrTables + 0x288 */
    /* 0x04 */ s8 arg;           /* for the move action: mode * 10 + type */
    /* 0x05 */ u8 pad05[3];
} BtlAiMgrActEntry; /* size 0x8 */

/* Action state of one side (side + 0x28). Reset by func_001B3F78. */
typedef struct BtlAiMgrAct {
    /* 0x00 */ s32 flags;        /* BTLAI_ACT_* */
    /* 0x04 */ s32 depth;        /* entries on the stack; the running one is stack[depth - 1] */
    /* 0x08 */ s32 phase;        /* index into the action's 4-entry phase table (0 init, 1 start, 2 run, 3 end) */
    /* 0x0C */ s32 unk0C[2];     /* (n/m) */
    /* 0x14 */ BtlAiMgrActEntry stack[8];
    /* 0x54 */ u8 unk54[0x44];   /* (n/m) */
} BtlAiMgrAct; /* size 0x98 */

#define BTLAI_ACT_PATH 0x02          /* following the path instead of heading straight for the target */
#define BTLAI_ACT_BLOCKED_AHEAD 0x04 /* the stage blocks the segment from the fighter to a point ahead of it */
#define BTLAI_ACT_BLOCKED 0x08       /* the stage blocks the segment from the fighter to the move target */
#define BTLAI_ACT_BLOCKED_ID 0x10    /* ... and the blocking thing has an id (BtlAiHit.id != -1) */

/* AI button bits as func_001BCB78 takes them; func_001BC968 turns them into battle button bits (BTLB_* of
   btl_input.h): 1 GUARD, 2 DASH, 4 BLAST, 8 RUSH, 0x10..0x80 UP DOWN LEFT RIGHT, 0x100 LOCKON, 0x200 CHARGE,
   0x800 ASCEND, 0x1000 DESCEND, 0x2000 R3. Only the ones this file uses are named. */
#define BTLAI_BTN_DASH 0x0002
#define BTLAI_BTN_UP 0x0010
#define BTLAI_BTN_CHARGE 0x0200
#define BTLAI_BTN_ASCEND 0x0800
#define BTLAI_BTN_DESCEND 0x1000

/* Path built by func_001B3A50 (move + 0x20). The last point is the next one to reach. */
typedef struct BtlAiMovePath {
    /* 0x000 */ BtlAiMovePoint pts[16];
    /* 0x100 */ s32 count;
} BtlAiMovePath; /* size 0x104 */

/* Move action state of one side (side + 0xC0). */
typedef struct BtlAiMoveWork {
    /* 0x000 */ f32 dist[5];     /* distance to keep from the opponent, by move type. Set by func_001BAF68 (n/m):
                                    [0] = both fighters' radius (func_002062F0) summed, [1] = d, [2] = d * 5,
                                    [3] = [4] = d * 10 with d = func_00205C58(side) - radius(side) */
    /* 0x014 */ u8 unk014[0x0C];
    /* 0x020 */ BtlAiMovePath path;
    /* 0x124 */ u8 unk124[0x0C];
    /* 0x130 */ s32 pathTimer;   /* frames until the path may be rebuilt (60 after a build) */
    /* 0x134 */ s32 type;        /* entry arg % 10: 0..4, selects dist[] and the steering */
    /* 0x138 */ u8 unk138[0x08];
    /* 0x140 */ BtlAiVec target;
    /* 0x150 */ f32 remain;      /* distance to the next path point / to the opponent when the action started */
    /* 0x154 */ s32 mode;        /* entry arg / 10 when that is 1 or 2 */
    /* 0x158 */ s32 dir;         /* result of BtlAiMove_PickDir (0..4). Written here, not read here */
    /* 0x15C */ s32 flags;       /* bit 0: end the action (set outside this file) */
    /* 0x160 */ u8 unk160[0x14];
    /* 0x174 */ s32 stuckTimer;  /* frames since the action started or a path point was reached; ends at 61 */
    /* 0x178 */ u8 unk178[0x30];
} BtlAiMoveWork; /* size 0x1A8 */

/* The virtual pad the AI writes (side + 0x268). Cleared by func_001BC918, filled by func_001BCB78. (n/m) */
typedef struct BtlAiMgrOutput {
    /* 0x00 */ s32 buttons;      /* BTLB_* */
    /* 0x04 */ f32 stickX;
    /* 0x08 */ f32 stickY;
    /* 0x0C */ u8 unk0C[0x4C];
} BtlAiMgrOutput; /* size 0x58 */

/* One side's CPU controller. */
typedef struct BtlAiMgrSide {
    /* 0x000 */ s32 side;        /* object id of the fighter (0 or 1); side ^ 1 is the opponent */
    /* 0x004 */ s32 aiType;      /* fighter member entry + 0x3C (BattleMember.aiType): index of typeTbl */
    /* 0x008 */ s32 cpuLevel;    /* fighter member entry + 0x38 (BattleMember.cpuLevel: 0..29, -1 dummy) */
    /* 0x00C */ u8 unk00C[0x0C];
    /* 0x018 */ void *param;     /* fighter object + 0x934; NULL: this side is not run */
    /* 0x01C */ u8 unk01C[0x08];
    /* 0x024 */ s32 flags;       /* bit 0: param is owned (Heap_Free on reset) */
    /* 0x028 */ BtlAiMgrAct act;
    /* 0x0C0 */ BtlAiMoveWork move;
    /* 0x268 */ BtlAiMgrOutput out;
    /* 0x2C0 */ u8 unk2C0[0x80];
    /* 0x340 */ s8 rate[14];     /* per virtual button (0..13): BtlAi_GetPairRate of the aiType table */
    /* 0x34E */ u8 unk34E[0x46];
    /* 0x394 */ u8 work[0x188];  /* cleared whenever aiType / cpuLevel are set */
    /* 0x51C */ u8 unk51C[4];
} BtlAiMgrSide; /* size 0x520 */

/* Per-state / per-action tables inside the AI data file (BtlAiMgrData.tables). */
typedef struct BtlAiMgrTables {
    /* 0x000 */ u8 unk000[0x288];
    /* 0x288 */ u16 actFlags[0x80];  /* (n/m) by action id */
    /* 0x388 */ s8 stateClass[1];    /* by fighter state (fighter + 0x974); 0, 10 and 11 are tested here */
} BtlAiMgrTables;

/* One aiType table (BtlAiMgrData.typeTbl[n]); only the two sub-tables this file passes on. */
typedef struct BtlAiMgrTypeTbl {
    /* 0x000 */ s32 unk000;
    /* 0x004 */ u8 unk004[0x2C0];
    /* 0x2C4 */ u8 unk2C4[1];
} BtlAiMgrTypeTbl;

/* The AI data file (section of gCommonRes, pointers fixed up by func_001BAD50). */
typedef struct BtlAiMgrData {
    /* 0x00 */ s32 size[1 + 8 + 32]; /* (n/m) byte sizes of the sections below */
    /* 0xA4 */ BtlAiMgrTables *tables;
    /* 0xA8 */ void *unkA8[8];       /* (n/m) */
    /* 0xC8 */ BtlAiMgrTypeTbl *typeTbl[32];
} BtlAiMgrData;

/* The manager block, Heap_Alloc'ed (0xA60 bytes) by BtlAiMgr_Init. */
typedef struct BtlAiMgr {
    /* 0x000 */ BtlAiMgrData *data;
    /* 0x004 */ f32 dist;        /* distance between the two fighters' centres, updated each frame */
    /* 0x008 */ f32 radiusSum;   /* (n/m) both fighters' func_002062F0 */
    /* 0x00C */ s32 sight;       /* BTLAI_SIGHT_*: stage line test between the two fighters */
    /* 0x010 */ BtlAiMgrSide side[2];
    /* 0xA50 */ s32 frame;       /* counts the frames the AI ran; cleared by func_001BAD50 */
    /* 0xA54 */ s32 flags;       /* bit 0: data is owned (Heap_Free on reset) */
    /* 0xA58 */ u8 padA58[8];
} BtlAiMgr; /* size 0xA60 */

#define BTLAI_SIGHT_BLOCKED 1
#define BTLAI_SIGHT_BLOCKED_ID 2

/* Partial view of a fighter (BtlChar_Get): the one field this file reads directly. */
typedef struct BtlAiMgrChr {
    /* 0x000 */ u8 unk000[0x444];
    /* 0x444 */ f32 yaw;         /* angle the stick directions are relative to */
} BtlAiMgrChr;

void BtlAiMgr_Reset(void);
void BtlAiMgr_ResetSide(s32 side);
void BtlAiMgr_Init(void);
void BtlAiMgr_Term(void);
void BtlAiMgr_BuildRates(BtlAiMgrSide *s);
s32 BtlAiMgr_GetType(s32 side);
void BtlAiMgr_SetType(s32 side, s32 aiType);
s32 BtlAiMgr_GetLevel(s32 side);
void BtlAiMgr_SetLevel(s32 side, s32 cpuLevel);
s32 BtlAiMgr_IsIdleState(void);
void BtlAiMgr_UpdateSight(void);
s32 BtlAiMgr_GetFrame(void);
void BtlAiMgr_Update(void);

s32 BtlAiMove_PickDir(BtlAiMgrSide *s, s32 mode);
void BtlAiMove_CalcTarget(BtlAiMgrSide *s);
s32 BtlAiMove_IsNear(BtlAiMgrSide *s, BtlAiVec *p, s32 flat);
s32 BtlAiMove_BuildPath(BtlAiMgrSide *s);
s32 BtlAiMove_Check(BtlAiMgrSide *s);
void BtlAiMove_Steer(BtlAiMgrSide *s, BtlAiVec *to, s32 kind);
void BtlAiMove_Hold(BtlAiMgrSide *s);
void BtlAiMove_Init(BtlAiMgrSide *s);
void BtlAiMove_Start(BtlAiMgrSide *s);
void BtlAiMove_Run(BtlAiMgrSide *s);
void BtlAiMove_End(BtlAiMgrSide *s);

#endif
