#include "common.h"
#include "battle/btl_seq.h"
#include "sys/heap.h"
#include "sys/adx.h"

/* The original object started at 0x215540 with eight text/subtitle drawing functions. They are
 * not decompiled yet, but have to live in this file so its .rodata lines up (see below). */
INCLUDE_ASM("asm/nonmatchings/battle/btl_seq", func_00215540);
INCLUDE_ASM("asm/nonmatchings/battle/btl_seq", func_002155F0);
INCLUDE_ASM("asm/nonmatchings/battle/btl_seq", func_002156E8);
INCLUDE_ASM("asm/nonmatchings/battle/btl_seq", func_002156F0);
INCLUDE_ASM("asm/nonmatchings/battle/btl_seq", func_00216508);
INCLUDE_ASM("asm/nonmatchings/battle/btl_seq", func_002166A0);
INCLUDE_ASM("asm/nonmatchings/battle/btl_seq", func_00216818);
INCLUDE_ASM("asm/nonmatchings/battle/btl_seq", func_002168F8);

/* Battle sequence state machine, 0x216AC0-0x2187E0. See battle/btl_seq.h for the state list.
 *
 * Object boundary: the first .rodata item this file emits (the tick table of BtlClock_Tick, 0x2F1AD8) follows
 * jtbl_002F1A80 of func_00215540 with no padding, and the jump tables after it sit at +0x18 from it. So the
 * original object did not start here: it also held 0x58 (mod 16: 8) bytes of earlier .rodata, i.e. at least
 * func_00215540..func_002168F8 (text drawing, 0x215540-0x216AC0). Linked on its own this file puts the tick
 * table at a 16-byte boundary and the jump tables 8 bytes too early. */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);

/* Local views of things owned by other modules (their headers are still moving). */
typedef struct BtlSeqBattleWork {
    /* 0x0000 */ u8 unk0[0x1938];
    /* 0x1938 */ BtlSeqResult result;
    /* 0x196C */ u8 unk196C[0x19F0 - 0x196C];
    /* 0x19F0 */ u64 flags;
} BtlSeqBattleWork;

/* BtlSeqBattleWork.flags bits used here */
#define BTL_FLAG_SKIP 0x100       /* simulation skipped this frame */
#define BTL_FLAG_DEMO 0x200       /* set by every non-fight state, cleared when the fighters are released */
#define BTL_FLAG_READY 0x400      /* set during the 2 s of state 2, cleared on entering state 3 */
#define BTL_FLAG_UNK2000 0x2000
#define BTL_FLAG_PAUSE 0x4000     /* pause menu open */
#define BTL_FLAG_RESTART 0x8000   /* restart the battle */

typedef struct BtlSeqObj {
    /* 0x000 */ u8 unk0[0xC];
    /* 0x00C */ s32 chara;  /* character id */
    /* 0x010 */ u8 unk10[0x93C - 0x10];
    /* 0x93C */ u8 *talkTbl; /* 4 bytes per opponent character id: {intro line or 0xFF, speaks second, win line or 0xFF, ?} */
} BtlSeqObj;

extern BtlSeqBattleWork *Battle_GetWork(void);
extern s32 Battle_GetMode(void);
extern BtlSeqResult *Battle_GetResult(void);   /* &Battle_GetWork()->result */
extern void func_001288F0(s32 winner, s32 reason); /* sets the result */
extern s32 func_00128960(void);             /* result.winner & BTL_RESULT_ABORT */
extern s32 Battle_IsRematchRequested(void);             /* result.reason & BTL_REASON_RESTART */
extern s32 func_001289B8(void);             /* result.winner & 3: a side won */
extern s32 func_001289E0(void);             /* split screen, mode 8, mode 1, or side 0 won */
extern s32 func_00128A78(void);             /* winning side: 0, or 1 when only bit 1 is set */
extern s32 func_00128AB0(void);             /* result.reason bit 0 */
extern s32 func_00128AD8(void);             /* result.reason bit 1 */
extern s32 func_00128B00(void);             /* result.reason bit 2 */
extern s32 func_00128B28(void);             /* result.reason bit 18 */
extern s32 func_00128B78(void);
extern s32 func_00128BA8(s32 side);         /* rule flag 0x3C of a side */
extern s32 func_0012A9E8(void);
extern s32 func_0012A9F8(void);
extern s32 func_0012AA18(void);
extern s32 func_0012AB58(void);             /* time limit in seconds (table D_002C3480) */
extern s32 func_0012AB90(void);             /* time limit is off */
extern s32 func_0012B1D0(s32 side);         /* object index of a side's fighter */
extern void func_0012C000(s32 side);
extern BtlSeqObj *BtlObj_Get(s32 idx);
extern void func_0024F5D8(BtlSeqObj *obj, s32 a, s32 line); /* mouth / talk animation */
extern void func_00267AC8(BtlSeqTimer *timer, f32 seconds, f32 from, f32 to);
extern s32 func_00267B00(BtlSeqTimer *timer); /* steps a timer, 1 when it ended */
extern void func_00209EE8(s32 side);        /* character flag 0xEF: entrance pose */
extern void func_00209F20(s32 side);        /* character flag 0xF0: end of entrance */
extern void func_00209F58(s32 side);        /* character flag 0xF1: win pose */
extern void func_00209F90(s32 side);        /* character flag 0xF2: lose pose */
extern s32 func_00209FC8(s32 side);         /* pose reached */
extern void func_0023DE60(s32 side, s32 cut); /* fighter camera cut */
extern void func_0023DE30(s32 a);
extern s32 func_0023DFF8(s32 cut);          /* stage camera cut */
extern s32 func_0023DBC0(void);             /* camera cut still playing */
extern s32 func_0023DC20(void);
extern void func_0023DCB8(void);            /* stop the camera cut */
extern void func_00244870(void);
extern void func_00244830(s32 a, f32 seconds);
extern s32 func_002592D8(void);
extern void func_00259360(void);
extern u8 *func_00259528(void);
extern void func_0022AB50(s32 id);          /* HUD announcement */
extern s32 func_00207090(void);             /* any character has flag 0x128 */
extern s32 func_00207270(s32 side);
extern s32 func_0020B8F0(s32 side);         /* character flag 7 */
extern s32 func_0020B878(s32 side);         /* every character of the side has no health */
extern s32 func_0022FB90(void);
extern void func_0022FBB0(s32 pad);
extern s32 func_0022FBD8(void);
extern s32 func_0022FC20(void);
extern s32 func_00212FF8(s32 a, s32 b);     /* pause / result menu update */
extern void func_00213220(void);
extern void func_00125170(s32 a, s32 b);
extern void func_00218A58(s32 on);          /* HUD visibility bits */
extern void func_00218AE8(s32 on);
extern void func_00218B08(s32 on);
extern void func_00218B30(s32 on);
extern void func_00218B58(s32 on);
extern void func_00218B80(s32 on);
extern void Fade_Start(s32 idx, s32 dir, f32 seconds);


s32 BtlSeqIntroTalk_Setup(BtlSeqTalkCtx *ctx);
s32 BtlSeqWinTalk_Setup(BtlSeqTalkCtx *ctx);
s32 BtlSeq_CanDraw(void);
void BtlSeq_JudgeByHealth(BtlSeqResult *result);
void BtlSeq_SetResultPad(void);
s32 BtlSeq_IsModeZero(void);

/* Returns the first state of a table that has an enter handler (0 when none has). */
s32 BtlSeq_FirstState(BtlSeqState *table) {
    s32 i;

    for (i = 0; i < BTL_SEQ_STATE_COUNT; i++) {
        if (table[i].enter != NULL) {
            return i;
        }
    }
    return 0;
}

/* Calls one handler of a state with the state's poll callback loaded into the context; -1 for an empty slot. */
s32 BtlSeq_Call(BtlSeqFunc func, s32 state) {
    if (func == NULL) {
        return -1;
    }
    gBtlSeq->ctx.poll = gBtlSeq->table[state].poll;
    return func(&gBtlSeq->ctx);
}

/* Clears the sequence, picks the state table for the battle mode and enters its first state. */
s32 BtlSeq_Reset(void) {
    memset(gBtlSeq, 0, sizeof(BtlSeq));
    BtlSeq_ResetClocks();
    switch (Battle_GetMode()) {
    case 1:
        gBtlSeq->table = gBtlSeqTblMode1;
        break;
    case 5:
    case 6:
    case 7:
        gBtlSeq->table = gBtlSeqTblMode5to7;
        break;
    default:
        gBtlSeq->table = gBtlSeqTblDefault;
        break;
    }
    gBtlSeq->state = BtlSeq_FirstState(gBtlSeq->table);
    Voice_Stop(0);
    Voice_Stop(1);
    return BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].enter, gBtlSeq->state);
}

/* Allocates the sequence and starts it. */
s32 BtlSeq_Init(void) {
    gBtlSeq = Heap_Alloc(sizeof(BtlSeq), 0x20, 0, HEAP_ANY);
    memset(gBtlSeq, 0, sizeof(BtlSeq));
    return BtlSeq_Reset();
}

/* Frees the sequence. */
void BtlSeq_Term(void) {
    Heap_Free(gBtlSeq);
    gBtlSeq = NULL;
}

/* Runs the current state's preUpdate handler (before the battle simulation). */
s32 BtlSeq_PreUpdate(void) {
    return BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].preUpdate, gBtlSeq->state);
}

/* Runs the current state's update handler and performs the state change it asks for; 1 = leave the battle. */
s32 BtlSeq_Update(void) {
    s32 next = BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].update, gBtlSeq->state);

    if (next != gBtlSeq->state) {
        BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].exit, gBtlSeq->state);
        gBtlSeq->state = next;
        if (gBtlSeq->state == BTL_SEQ_EXIT) {
            if (Battle_IsRematchRequested() || (Battle_GetMode() == 6 && func_00128B00())) {
                Battle_GetWork()->flags |= BTL_FLAG_RESTART;
                return 0;
            }
            return 1;
        }
        if (gBtlSeq->table[gBtlSeq->state].enter == NULL) {
            gBtlSeq->state = BTL_SEQ_END;
        }
        BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].enter, gBtlSeq->state);
    }
    return 0;
}

/* Returns the battle clock. */
BtlClock *BtlSeq_GetClock(void) {
    return &gBtlSeq->clock;
}

/* Returns the second clock. */
BtlClock *BtlSeq_GetSubClock(void) {
    return &gBtlSeq->subClock;
}

/* Returns the current state. */
s32 BtlSeq_GetState(void) {
    return gBtlSeq->state;
}

/* True while the fight itself is running (state 3). */
s32 BtlSeq_IsFighting(void) {
    return BtlSeq_GetState() == BTL_SEQ_FIGHT;
}

/* True during the finish announcement (state 4). */
s32 BtlSeq_IsFinish(void) {
    return BtlSeq_GetState() == BTL_SEQ_FINISH;
}

/* Runs the current state's extra handler (mode 1: skip the talk). */
s32 BtlSeq_CallExtra(void) {
    return BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].extra, gBtlSeq->state);
}

/* Turns the battle-end check (and the clocks) off or on. */
void BtlSeq_SetEndCheckOff(s32 off) {
    gBtlSeq->endCheckOff = off;
}

/* Returns whether the battle-end check is off. */
s32 BtlSeq_GetEndCheckOff(void) {
    return gBtlSeq->endCheckOff;
}

/* Zeroes a clock. */
void BtlClock_Clear(BtlClock *clock) {
    memset(clock, 0, sizeof(BtlClock));
}

/* Advances a clock by one tick (34, 32, 34 ms in turn), stopping at 9:59:59.999. */
s32 BtlClock_Tick(BtlClock *clock) {
    s32 tickMs[3] = { 34, 32, 34 };

    s32 add = tickMs[clock->ticks++ % 3];

    clock->ms += add;
    if (clock->hours >= 9 && clock->minutes >= 59 && clock->seconds >= 59 && clock->ms >= 999) {
        clock->hours = 9;
        clock->minutes = 59;
        clock->seconds = 59;
        clock->ms = 999;
    } else {
        if (clock->ms >= 1000) {
            clock->ms -= 1000;
            clock->seconds++;
        }
        if (clock->seconds >= 60) {
            clock->seconds -= 60;
            clock->minutes++;
        }
        if (clock->minutes >= 60) {
            clock->minutes -= 60;
            clock->hours++;
        }
    }
    return 1;
}

/* Clears both clocks and loads the time limit into the battle clock. */
void BtlSeq_ResetClocks(void) {
    BtlClock *clock = BtlSeq_GetClock();

    BtlClock_Clear(clock);
    clock->timeLeft = func_0012AB58();
    BtlClock_Clear(BtlSeq_GetSubClock());
}

/* Ticks both clocks and updates the time left; 1 when the time limit is reached. */
s32 BtlSeq_TickClocks(void) {
    BtlClock *clock;

    BtlSeq_GetClock();
    BtlClock_Tick(BtlSeq_GetSubClock());
    clock = BtlSeq_GetClock();
    BtlClock_Tick(clock);
    if (func_0012AB90()) {
        return 0;
    }
    if (clock->minutes * 60 + clock->seconds >= func_0012AB58()) {
        clock->timeLeft = 0;
        return 1;
    }
    clock->timeLeft = func_0012AB58() - (clock->minutes * 60 + clock->seconds);
    return 0;
}

/* Restarts the second clock. */
void BtlSeq_ResetSubClock(void) {
    BtlClock_Clear(BtlSeq_GetSubClock());
}

/* Seconds left of the time limit, -1 when there is none. */
s32 BtlSeq_GetTimeLeft(void) {
    if (func_0012AB90()) {
        return -1;
    }
    return BtlSeq_GetClock()->timeLeft;
}

/* Stops both fighters' talk animation and both voice players. */
void BtlSeq_StopTalk(void) {
    func_0024F5D8(BtlObj_Get(func_0012B1D0(0)), 0, -1);
    func_0024F5D8(BtlObj_Get(func_0012B1D0(1)), 0, -1);
    Voice_Stop(0);
    Voice_Stop(1);
}

/* Chooses the two entrance lines: the pair's special dialogue when both have a table entry, else random. */
s32 BtlSeqIntroTalk_Setup(BtlSeqTalkCtx *ctx) {
    BtlSeqObj *obj0 = BtlObj_Get(func_0012B1D0(0));
    u8 *tbl0 = obj0->talkTbl;
    BtlSeqObj *obj1 = BtlObj_Get(func_0012B1D0(1));
    u8 *tbl1 = obj1->talkTbl;

    if (tbl0 != NULL && tbl1 != NULL) {
        if (tbl0[obj1->chara * 4] != 0xFF) {
            if (tbl0[obj1->chara * 4 + 1] == 0) {
                ctx->side[0] = 0;
                ctx->side[1] = 1;
                ctx->chara[0] = obj0->chara;
                ctx->chara[1] = obj1->chara;
                ctx->line[0] = tbl0[obj1->chara * 4] + 6;
                ctx->line[1] = tbl1[obj0->chara * 4] + 6;
            } else {
                ctx->side[1] = 0;
                ctx->side[0] = 1;
                ctx->chara[0] = obj1->chara;
                ctx->chara[1] = obj0->chara;
                ctx->line[0] = tbl1[obj0->chara * 4] + 6;
                ctx->line[1] = tbl0[obj1->chara * 4] + 6;
            }
        } else {
            ctx->side[0] = 0;
            ctx->side[1] = 1;
            ctx->chara[0] = obj0->chara;
            ctx->chara[1] = obj1->chara;
            ctx->line[0] = rand() % 2;
            ctx->line[1] = rand() % 2;
        }
    } else {
        ctx->side[0] = 0;
        ctx->side[1] = 1;
        ctx->chara[0] = obj0->chara;
        ctx->chara[1] = obj1->chara;
        ctx->line[0] = rand() % 2;
        ctx->line[1] = rand() % 2;
    }
    ctx->step = 0;
    func_00267AC8(&ctx->timer, 10.0f, 0.0f, 1.0f);
    return 1;
}

/* State 1 enter: prepares the entrance lines; mode 1 has none and waits for the skip request. */
s32 BtlSeqIntroTalk_Enter(BtlSeqTalkCtx *ctx) {
    if (Battle_GetMode() == 1) {
        func_0012C000(0);
        func_0012C000(1);
        ctx->skip = 0;
        ctx->step = 99;
    } else {
        BtlSeqIntroTalk_Setup(ctx);
    }
    Battle_GetWork()->flags |= BTL_FLAG_DEMO;
    return 1;
}

/* State 1 preUpdate: at steps 0 and 2 starts a speaker's pose, camera cut, voice line and talk animation. */
s32 BtlSeqIntroTalk_PreUpdate(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
        func_00209EE8(ctx->side[0]);
        func_00267AC8(&ctx->timer, 10.0f, 0.0f, 1.0f);
        func_0023DE60(ctx->side[0], 0);
        Voice_PlayChara(ctx->side[0], ctx->chara[0], ctx->line[0]);
        func_0024F5D8(BtlObj_Get(func_0012B1D0(ctx->side[0])), 2, ctx->line[0]);
        ctx->step++;
        break;
    case 2:
        func_00209EE8(ctx->side[1]);
        func_00267AC8(&ctx->timer, 10.0f, 0.0f, 1.0f);
        func_0023DE60(ctx->side[1], 0);
        Voice_PlayChara(ctx->side[1], ctx->chara[1], ctx->line[1]);
        func_0024F5D8(BtlObj_Get(func_0012B1D0(ctx->side[1])), 2, ctx->line[1]);
        ctx->step++;
        break;
    case 3:
        break;
    }
    return 1;
}

/* State 1 update: waits for each line to end (voice stopped or 10 s), or for the skip; then goes to state 2. */
s32 BtlSeqIntroTalk_Update(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
    case 1:
        if (func_00209FC8(ctx->side[0])) {
            if (Voice_IsStopped(ctx->side[0])) {
                func_00244870();
                func_00244830(1, 1.0f);
                ctx->step++;
            } else if (func_00267B00(&ctx->timer)) {
                func_00244870();
                func_00244830(1, 1.0f);
                ctx->step++;
            }
        }
        break;
    case 2:
    case 3:
        if (func_00209FC8(ctx->side[1])) {
            if (Voice_IsStopped(ctx->side[1]) || func_00267B00(&ctx->timer)) {
                func_00244870();
                func_00244830(1, 1.0f);
                ctx->step++;
            }
        }
        break;
    case 4:
        return 2;
    case 99:
        if (ctx->skip) {
            func_00244870();
            func_00244830(1, 1.0f);
            goto done;
        }
        break;
    }
    if (ctx->poll != NULL) {
        if (ctx->poll()) {
            if (ctx->step != 4) {
                func_00244870();
                func_00244830(1, 1.0f);
                if (func_002592D8()) {
                    func_00259360();
                }
            }
done:
            BtlSeq_StopTalk();
            return 2;
        }
    }
    return 1;
}

/* State 1 exit: stops the camera cut and ends both entrance poses (not in mode 1). */
s32 BtlSeqIntroTalk_Exit(BtlSeqTalkCtx *ctx) {
    if (Battle_GetMode() != 1) {
        func_0023DCB8();
        func_00209F20(ctx->side[0]);
        func_00209F20(ctx->side[1]);
    }
    return 1;
}

/* State 1 extra (mode 1): asks the state to end. */
s32 BtlSeqIntroTalk_Skip(BtlSeqTalkCtx *ctx) {
    s32 mode = Battle_GetMode();

    if (mode == 1) {
        ctx->skip = mode;
    }
    return 1;
}

/* Chooses the speaker, line and announcement of the winner scene. */
s32 BtlSeqWinTalk_Setup(BtlSeqTalkCtx *ctx) {
    s32 winner = func_00128A78();
    s32 loser = winner == 0;
    BtlSeqObj *winObj = BtlObj_Get(func_0012B1D0(winner));
    u8 *winTbl = winObj->talkTbl;
    BtlSeqObj *loseObj = BtlObj_Get(func_0012B1D0(loser));
    u8 *loseTbl = loseObj->talkTbl;

    if (func_001289E0()) {
        if (winTbl == NULL || loseTbl == NULL || func_00207270(winner) || func_00207270(loser)) {
            ctx->side[0] = winner;
            if (func_00207270(winner)) {
                ctx->chara[0] = 0x56;
            } else {
                ctx->chara[0] = winObj->chara;
            }
            ctx->line[0] = rand() % 2 + 3;
            ctx->step = 0;
        } else {
            if (winTbl[loseObj->chara * 4 + 2] != 0xFF) {
                ctx->side[0] = winner;
                ctx->chara[0] = winObj->chara;
                ctx->line[0] = winTbl[loseObj->chara * 4 + 2] + 0x1E;
                ctx->step = 0;
            } else {
                ctx->side[0] = winner;
                ctx->chara[0] = winObj->chara;
                ctx->line[0] = rand() % 2 + 3;
                ctx->step = 0;
            }
        }
        func_0022AB50(6);
    } else {
        ctx->side[0] = loser;
        if (func_00207270(loser)) {
            ctx->chara[0] = 0x56;
        } else {
            ctx->chara[0] = loseObj->chara;
        }
        ctx->step = 0;
        ctx->line[0] = 0x2A;
        func_0022AB50(7);
    }
    func_00267AC8(&ctx->timer, 10.0f, 0.0f, 1.0f);
    return 1;
}

/* State 5 enter: prepares the winner scene and hides HUD parts; mode 1 can be told to wait for the skip instead. */
s32 BtlSeqWinTalk_Enter(BtlSeqTalkCtx *ctx) {
    Battle_GetWork()->flags |= BTL_FLAG_DEMO;
    func_00244870();
    func_00244830(1, 1.0f);
    if (Battle_GetMode() == 1) {
        if (*func_00259528() & 0x10) {
            ctx->skip = 0;
            ctx->step = 99;
        } else {
            BtlSeqWinTalk_Setup(ctx);
        }
        func_00218AE8(0);
        func_00218B08(0);
        func_00218B58(0);
        func_00218B80(0);
        func_00218B30(0);
    } else {
        BtlSeqWinTalk_Setup(ctx);
        func_00218AE8(0);
        func_00218B08(0);
        func_00218B58(0);
        func_00218B80(0);
    }
    return 1;
}

/* State 5 preUpdate: step 0 starts the pose, camera cut and voice line; step 2 starts a 1.5 s hold. */
s32 BtlSeqWinTalk_PreUpdate(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
        if (func_001289E0()) {
            func_00209F58(ctx->side[0]);
            func_0023DE60(ctx->side[0], 1);
            func_0023DE30(1);
        } else {
            func_00209F90(ctx->side[0]);
            func_0023DE60(ctx->side[0], 2);
        }
        func_00267AC8(&ctx->timer, 10.0f, 0.0f, 1.0f);
        Voice_PlayChara(ctx->side[0], ctx->chara[0], ctx->line[0]);
        func_0024F5D8(BtlObj_Get(func_0012B1D0(ctx->side[0])), 2, ctx->line[0]);
        ctx->step++;
        break;
    case 2:
        func_00267AC8(&ctx->timer, 1.5f, 0.0f, 1.0f);
        ctx->step++;
        break;
    case 3:
        break;
    }
    return 5;
}

/* State 5 update: waits for the line (or 10 s), holds 1.5 s, then goes to state 6; the poll callback skips. */
s32 BtlSeqWinTalk_Update(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
    case 1:
        if (func_00209FC8(ctx->side[0])) {
            if (Voice_IsStopped(ctx->side[0]) || func_00267B00(&ctx->timer)) {
                ctx->step = 2;
            }
        }
        break;
    case 2:
    case 3:
        if (func_00267B00(&ctx->timer)) {
            ctx->step = 4;
        }
        break;
    case 4:
        return 6;
    case 99:
        if (ctx->skip) {
            goto done;
        }
        break;
    }
    if (ctx->poll != NULL) {
        if (ctx->poll()) {
done:
            BtlSeq_StopTalk();
            return 6;
        }
    }
    return 5;
}

/* State 5 exit: stops the talk (not in mode 1). */
s32 BtlSeqWinTalk_Exit(BtlSeqTalkCtx *ctx) {
    if (Battle_GetMode() != 1) {
        BtlSeq_StopTalk();
    }
    return 1;
}

/* State 5 extra (mode 1): asks the state to end. */
s32 BtlSeqWinTalk_Skip(BtlSeqTalkCtx *ctx) {
    s32 mode = Battle_GetMode();

    if (mode == 1) {
        ctx->skip = mode;
    }
    return 1;
}

/* State 0 enter: restarts the step counter and holds the fighters. */
s32 BtlSeqStageIntro_Enter(BtlSeqWaitCtx *ctx) {
    ctx->step = 0;
    Battle_GetWork()->flags |= BTL_FLAG_DEMO;
    return 1;
}

/* State 0 preUpdate: at steps 0, 2 and 4 starts stage camera cut 0, 1 and 2. */
s32 BtlSeqStageIntro_PreUpdate(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
        func_0023DFF8(0);
        ctx->step++;
        break;
    case 2:
        func_0023DFF8(1);
        ctx->step++;
        break;
    case 4:
        func_0023DFF8(2);
        ctx->step++;
        break;
    case 1:
    case 3:
    case 5:
    case 6:
        break;
    }
    return 0;
}

/* State 0 update: steps on when a camera cut ends; goes to state 1 after the third or on the poll callback. */
s32 BtlSeqStageIntro_Update(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
    case 1:
    case 2:
    case 3:
        if (!func_0023DBC0()) {
            func_00244870();
            func_00244830(1, 1.0f);
            ctx->step++;
        }
        break;
    case 4:
    case 5:
        if (!func_0023DBC0()) {
            ctx->step++;
            return 1;
        }
        break;
    case 6:
        return 1;
    }
    if (ctx->poll != NULL) {
        if (ctx->poll()) {
            return 1;
        }
    }
    return 0;
}

/* State 0 exit: stops a camera cut that is still playing. */
s32 BtlSeqStageIntro_Exit(BtlSeqWaitCtx *ctx) {
    if (func_0023DBC0()) {
        func_0023DCB8();
    }
    func_00244870();
    func_00244830(1, 1.0f);
    return 1;
}

/* True when the battle ended by time up in mode 0: the only case where equal health is a draw. */
s32 BtlSeq_CanDraw(void) {
    if (func_00128AD8() && Battle_GetMode() == 0) {
        return 1;
    }
    return 0;
}

/* Picks the winner from the two sides' health; equal health falls back to a rule flag, a draw, or rand(). */
void BtlSeq_JudgeByHealth(BtlSeqResult *result) {
    if (result->health[0] == result->health[1]) {
        if (func_00128BA8(0)) {
            result->winner = BTL_RESULT_WIN_P1;
        } else if (func_00128BA8(1)) {
            result->winner = BTL_RESULT_WIN_P2;
        } else if (BtlSeq_CanDraw()) {
            result->winner = BTL_RESULT_DRAW;
        } else if (Battle_GetMode() == 8) {
            result->winner = BTL_RESULT_WIN_P2;
        } else if (rand() & 1) {
            result->winner = BTL_RESULT_WIN_P1;
        } else {
            result->winner = BTL_RESULT_WIN_P2;
        }
    } else if (result->health[1] < result->health[0]) {
        result->winner = BTL_RESULT_WIN_P1;
    } else {
        result->winner = BTL_RESULT_WIN_P2;
    }
}

/* Ticks the clocks and decides whether the battle is over, filling the result block; 1 when it is. */
#if 0
/* Not matching: same instructions per block, but this compiler merges the three "set reason + judge by health"
 * blocks into one (cross-jumping) where the original keeps three copies, which also shifts every branch. */
s32 BtlSeq_CheckBattleEnd(void) {
    BtlSeqResult *result = Battle_GetResult();
    s32 timeUp;

    if (result->winner & 0x1F) {
        return 1;
    }
    if (BtlSeq_GetEndCheckOff()) {
        return 0;
    }
    if (Battle_GetWork()->flags & BTL_FLAG_SKIP) {
        return 0;
    }
    if (Battle_GetWork()->flags & BTL_FLAG_UNK2000) {
        return 0;
    }
    if (func_00207090()) {
        return 0;
    }
    timeUp = BtlSeq_TickClocks();
    if (Battle_GetMode() == 1) {
        return 0;
    }
    if (timeUp) {
        result->reason = BTL_REASON_TIME_UP;
        BtlSeq_JudgeByHealth(result);
        return 1;
    }
    if (func_0020B8F0(0) && func_0020B8F0(1)) {
        result->reason = BTL_REASON_FLAG7;
        BtlSeq_JudgeByHealth(result);
        return 1;
    }
    if (func_0020B8F0(1)) {
        result->winner = BTL_RESULT_WIN_P1;
        result->reason = BTL_REASON_FLAG7;
        return 1;
    }
    if (func_0020B8F0(0)) {
        result->winner = BTL_RESULT_WIN_P2;
        result->reason = BTL_REASON_FLAG7;
        return 1;
    }
    if (func_0020B878(0) && func_0020B878(1)) {
        result->reason = BTL_REASON_KO;
        BtlSeq_JudgeByHealth(result);
        return 1;
    }
    if (func_0020B878(1)) {
        result->winner = BTL_RESULT_WIN_P1;
        result->reason = BTL_REASON_KO;
        return 1;
    }
    if (func_0020B878(0)) {
        result->winner = BTL_RESULT_WIN_P2;
        result->reason = BTL_REASON_KO;
        return 1;
    }
    if (func_0012A9E8() && func_0012AA18()) {
        result->reason = BTL_REASON_BIT18;
        result->winner = BTL_RESULT_OTHER;
        return 1;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/btl_seq", BtlSeq_CheckBattleEnd);
#endif

/* State 3 enter: releases the fighters (clears the demo and ready flags); mode 8 shows the HUD. */
s32 BtlSeqFight_Enter(BtlSeqWaitCtx *ctx) {
    Battle_GetWork()->flags &= ~BTL_FLAG_DEMO;
    Battle_GetWork()->flags &= ~BTL_FLAG_READY;
    if (Battle_GetMode() == 8) {
        func_00218A58(1);
    }
    return 1;
}

/* State 3 preUpdate: nothing. */
s32 BtlSeqFight_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 3;
}

/* State 3 update: opens / runs the pause menu, then checks for the end of the battle (-> 4, or 6 without a finish scene). */
s32 BtlSeqFight_Update(BtlSeqWaitCtx *ctx) {
    if (Battle_GetMode() != 8) {
        if (ctx->poll != NULL && !(Battle_GetWork()->flags & BTL_FLAG_PAUSE) && ctx->poll()) {
            if (Battle_GetMode() == 7) {
                func_001288F0(BTL_RESULT_ABORT, 0);
            } else if (func_0012A9E8() && func_0012A9F8() && func_0022FC20() == 1) {
            } else if (Battle_GetMode() == 1 && func_002592D8()) {
                func_00259360();
            } else {
                Battle_GetWork()->flags |= BTL_FLAG_PAUSE | BTL_FLAG_SKIP;
                Adx_PauseSeVoice();
                func_00125170(4, 1);
            }
        }
        if (Battle_GetWork()->flags & BTL_FLAG_PAUSE) {
            if (func_00212FF8(func_0022FBD8(), 0) == 0) {
                Battle_GetWork()->flags &= ~(BTL_FLAG_PAUSE | BTL_FLAG_SKIP);
                func_00125170(4, 0);
            }
        }
    }
    if (BtlSeq_CheckBattleEnd()) {
        if (Battle_GetMode() == 7) {
            return 6;
        }
        return func_00128960() ? 6 : 4;
    }
    return 3;
}

/* State 3 exit: nothing. */
s32 BtlSeqFight_Exit(BtlSeqWaitCtx *ctx) {
    return 1;
}

/* State 2 enter: holds the fighters, starts the 0.8 s wait and shows the HUD. */
s32 BtlSeqReady_Enter(BtlSeqWaitCtx *ctx) {
    Battle_GetWork()->flags |= BTL_FLAG_DEMO;
    ctx->step = 0;
    func_00267AC8(&ctx->timer, 0.8f, 0.0f, 1.0f);
    func_00218A58(1);
    return 1;
}

/* State 2 preUpdate: nothing. */
s32 BtlSeqReady_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 2;
}

/* State 2 update: after 0.8 s shows announcement 0 and releases the fighters; 2 s later goes to state 3. */
s32 BtlSeqReady_Update(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
        if (func_00267B00(&ctx->timer)) {
            func_0022AB50(0);
            Battle_GetWork()->flags &= ~BTL_FLAG_DEMO;
            Battle_GetWork()->flags |= BTL_FLAG_READY;
            func_00267AC8(&ctx->timer, 2.0f, 0.0f, 1.0f);
            ctx->step++;
        }
        break;
    case 1:
        if (func_00267B00(&ctx->timer)) {
            return 3;
        }
        break;
    }
    return 2;
}

/* State 2 exit: announcement 1. */
s32 BtlSeqReady_Exit(BtlSeqWaitCtx *ctx) {
    func_0022AB50(1);
    return 1;
}

/* Gives the result menu to pad 0 when one pad plays, else to the winning side. */
void BtlSeq_SetResultPad(void) {
    if (func_0022FB90() == 1) {
        func_0022FBB0(0);
    } else {
        func_0022FBB0(func_00128A78());
    }
}

/* True in battle mode 0. */
s32 BtlSeq_IsModeZero(void) {
    return Battle_GetMode() == 0;
}

/* State 6 enter: mode 0 with a finished battle opens the result menu (step 0); otherwise starts the 1.2 s fade out (step 1). */
s32 BtlSeqEnd_Enter(BtlSeqWaitCtx *ctx) {
    Battle_GetWork()->flags |= BTL_FLAG_DEMO;
    if (func_00128960()) {
        Fade_Start(0, 0, 1.0f);
        func_00267AC8(&ctx->timer, 1.2f, 0.0f, 1.0f);
        ctx->step = 1;
    } else if (BtlSeq_IsModeZero()) {
        ctx->step = 0;
        BtlSeq_SetResultPad();
    } else {
        Fade_Start(0, 0, 1.0f);
        func_00267AC8(&ctx->timer, 1.2f, 0.0f, 1.0f);
        ctx->step = 1;
    }
    return 1;
}

/* State 6 preUpdate: nothing. */
s32 BtlSeqEnd_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 6;
}

/* State 6 update: step 0 runs the result menu until it aborts the battle, step 1 waits for the fade and returns 99. */
s32 BtlSeqEnd_Update(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
        func_00212FF8(func_0022FBD8(), 1);
        func_00213220();
        if (func_00128960()) {
            Fade_Start(0, 0, 1.0f);
            func_00267AC8(&ctx->timer, 1.2f, 0.0f, 1.0f);
            ctx->step = 1;
        }
        break;
    case 1:
        if (func_00267B00(&ctx->timer)) {
            return BTL_SEQ_EXIT;
        }
        break;
    }
    return 6;
}

/* State 6 exit: stops the camera cut. */
s32 BtlSeqEnd_Exit(BtlSeqWaitCtx *ctx) {
    if (func_0023DC20()) {
        func_0023DCB8();
    }
    func_0023DE30(0);
    return 1;
}

/* State 4 enter: holds the fighters, starts the 3.5 s wait and shows the announcement for the finish reason. */
s32 BtlSeqFinish_Enter(BtlSeqWaitCtx *ctx) {
    BtlSeqTimer *timer;

    Battle_GetWork()->flags |= BTL_FLAG_DEMO;
    timer = &ctx->timer;
    func_00267AC8(timer, 3.5f, 0.0f, 1.0f);
    if (func_00128AB0()) {
        if (func_00128B78()) {
            func_0022AB50(3);
        } else {
            func_0022AB50(2);
        }
    } else if (func_00128AD8()) {
        func_0022AB50(5);
    } else if (func_00128B00()) {
        func_0022AB50(4);
    } else if (func_00128B28()) {
        func_00267AC8(timer, 1.1f, 0.0f, 1.0f);
    }
    return 1;
}

/* State 4 preUpdate: nothing. */
s32 BtlSeqFinish_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 4;
}

/* State 4 update: when the wait ends goes to the winner scene (5) if a side won and the mode is not 8, else to 6. */
s32 BtlSeqFinish_Update(BtlSeqWaitCtx *ctx) {
    if (func_00267B00(&ctx->timer)) {
        if (Battle_GetMode() != 8 && func_001289B8()) {
            return 5;
        }
        return 6;
    }
    return 4;
}

/* State 4 exit: nothing. */
s32 BtlSeqFinish_Exit(BtlSeqWaitCtx *ctx) {
    return 1;
}
