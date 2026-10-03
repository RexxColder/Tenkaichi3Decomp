#ifndef BATTLE_BTL_SEQ_H
#define BATTLE_BTL_SEQ_H

#include "types.h"

/*
 * Battle sequence: the state machine that drives one match from the stage intro to the fade out.
 * Source range 0x216AC0-0x2187E0 (core, clocks, win judgement and every state handler); src/battle/btl_seq.c
 * also holds the skill-list text of the pause menu, 0x215420-0x216AC0 (BtlText_*, at the end of this header).
 *
 * The frame loop calls BtlSeq_PreUpdate() before the battle simulation and BtlSeq_Update() after it.
 * BtlSeq_Update() returns 1 when the battle scene has to be left.
 *
 * States (the index is the same in the three tables; a table only leaves slots empty):
 *
 *   0 BTL_SEQ_STAGE_INTRO  stage fly-through, three camera cuts. Default table only.
 *                          -> 1 when the last cut ends or the skip callback fires.
 *   1 BTL_SEQ_INTRO_TALK   the two fighters' entrance poses and voice lines (special dialogue when the
 *                          pair has one, else a random line 0/1 each). Mode 1 plays nothing and waits
 *                          for the "extra" call. -> 2
 *   2 BTL_SEQ_READY        0.8 s wait, then announcement 0 for 2.0 s with the fighters released. -> 3
 *   3 BTL_SEQ_FIGHT        the match. Runs the clocks, the pause request and BtlSeq_CheckBattleEnd().
 *                          -> 4 when the battle is decided, -> 6 directly when the result has the
 *                          "aborted" bit (BTL_RESULT_ABORT) or the mode is 7.
 *   4 BTL_SEQ_FINISH       the finish announcement (K.O. / time up / ...), 3.5 s (1.1 s for reason bit 18).
 *                          -> 5 when a side won and the mode is not 8, else -> 6.
 *   5 BTL_SEQ_WIN_TALK     winner's pose and voice line. Default table and mode 1 only. -> 6
 *   6 BTL_SEQ_END          result handling and 1.2 s fade out. -> 99
 *  99 BTL_SEQ_EXIT         not a table entry: BtlSeq_Update() stops there. It returns 1 (leave the battle
 *                          scene), unless a restart was asked for (result reason bits 0x18000, or mode 6
 *                          with reason bit 2), in which case it sets battle flag 0x8000 and returns 0.
 *
 * A state whose `enter` slot is empty is never entered: BtlSeq_Update() goes to state 6 instead, and
 * BtlSeq_Reset() starts at the first non-empty slot.
 *
 * Table by Battle_GetMode():
 *   mode 1        gBtlSeqTblMode1     1 2 3 4 5 6   (no stage intro; states 1 and 5 have an `extra` handler)
 *   modes 5, 6, 7 gBtlSeqTblMode5to7      2 3 4   6   (no intro, no talk, no winner scene)
 *   anything else gBtlSeqTblDefault   0 1 2 3 4 5 6
 */

enum {
    BTL_SEQ_STAGE_INTRO = 0,
    BTL_SEQ_INTRO_TALK = 1,
    BTL_SEQ_READY = 2,
    BTL_SEQ_FIGHT = 3,
    BTL_SEQ_FINISH = 4,
    BTL_SEQ_WIN_TALK = 5,
    BTL_SEQ_END = 6,
    BTL_SEQ_STATE_COUNT = 7,
    BTL_SEQ_EXIT = 99
};

/* Every handler gets a pointer to BtlSeq.ctx and returns an s32. */
typedef s32 (*BtlSeqFunc)(void *ctx);
/* The per-state argument: a "skip / pause requested" poll, or NULL. */
typedef s32 (*BtlSeqPollFunc)(void);

/* One state of a table. */
typedef struct BtlSeqState {
    /* 0x00 */ BtlSeqFunc enter;     /* on entering the state; NULL marks the state as absent */
    /* 0x04 */ BtlSeqFunc preUpdate; /* every frame, before the battle simulation */
    /* 0x08 */ BtlSeqFunc update;    /* every frame, after it; returns the next state */
    /* 0x0C */ BtlSeqFunc exit;      /* on leaving the state */
    /* 0x10 */ BtlSeqFunc extra;     /* BtlSeq_CallExtra(); only mode 1 has any (skip the talk) */
    /* 0x14 */ BtlSeqPollFunc poll;  /* copied to BtlSeq.ctx.poll before every handler call */
} BtlSeqState; /* size 0x18 */

/* Linear ramp used as a frame timer (func_00267AC8 starts it, func_00267B00 steps it). */
typedef struct BtlSeqTimer {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 state;   /* 0 running, -1 makes the step function report "done" */
    /* 0x08 */ f32 frames;  /* frames left: seconds * 30 */
    /* 0x0C */ f32 step;    /* (to - from) / frames */
    /* 0x10 */ f32 value;
    /* 0x14 */ f32 target;
} BtlSeqTimer; /* size 0x18 */

/* Context of the states that only wait: READY, FINISH, END. */
typedef struct BtlSeqWaitCtx {
    /* 0x00 */ BtlSeqPollFunc poll;
    /* 0x04 */ s32 step;
    /* 0x08 */ BtlSeqTimer timer;
} BtlSeqWaitCtx;

/* Context of INTRO_TALK and WIN_TALK. */
typedef struct BtlSeqTalkCtx {
    /* 0x00 */ BtlSeqPollFunc poll;
    /* 0x04 */ s32 step;       /* 0 start 1st line, 1 wait, 2 start 2nd line, 3 wait, 4 done, 99 wait for skip */
    /* 0x08 */ s32 side[2];    /* side (0/1) speaking first / second; WIN_TALK only uses [0] = winner */
    /* 0x10 */ s32 line[2];    /* voice line of each speaker */
    /* 0x18 */ s32 chara[2];   /* character id of each speaker */
    /* 0x20 */ BtlSeqTimer timer; /* 10 s time-out of a line */
    /* 0x38 */ s32 skip;       /* set by the `extra` handler in mode 1 */
} BtlSeqTalkCtx;

/* A running clock. One tick is 34, 32, 34 ms in turn: 100 ms per 3 frames, 30 ticks per second. */
typedef struct BtlClock {
    /* 0x00 */ u32 ticks;
    /* 0x04 */ s16 hours;    /* stops at 9:59:59.999 */
    /* 0x06 */ s16 minutes;
    /* 0x08 */ s16 seconds;
    /* 0x0A */ s16 ms;
    /* 0x0C */ s16 timeLeft; /* battle clock only: seconds left of the time limit */
    /* 0x0E */ s16 unkE;
} BtlClock; /* size 0x10 */

typedef struct BtlSeq {
    /* 0x000 */ s32 state;
    /* 0x004 */ union {
        BtlSeqPollFunc poll; /* +0x004: BtlSeqState.poll of the current state */
        BtlSeqWaitCtx wait;
        BtlSeqTalkCtx talk;
        u8 raw[0x100];
    } ctx;                   /* not cleared between states */
    /* 0x104 */ BtlSeqState *table;
    /* 0x108 */ BtlClock clock;    /* time since the fight started, compared with the time limit */
    /* 0x118 */ BtlClock subClock; /* second clock, restarted by BtlSeq_ResetSubClock() */
    /* 0x128 */ s32 endCheckOff;   /* non-zero: BtlSeq_CheckBattleEnd() does nothing (clocks stop too) */
} BtlSeq; /* size 0x12C */

/* BtlSeqResult.winner */
#define BTL_RESULT_WIN_P1 0x01
#define BTL_RESULT_WIN_P2 0x02
#define BTL_RESULT_DRAW   0x04
#define BTL_RESULT_ABORT  0x08 /* battle left without a finish scene (set from the pause menu in mode 7) */
#define BTL_RESULT_OTHER  0x10 /* set together with reason 0x40000 */
/* BtlSeqResult.reason */
#define BTL_REASON_KO      0x00001 /* every character of a side has no health left */
#define BTL_REASON_TIME_UP 0x00002
#define BTL_REASON_FLAG7   0x00004 /* character flag 7 set on the active fighter (ring out / forced defeat) */
#define BTL_REASON_RESTART 0x18000 /* either bit: restart the battle instead of leaving */
#define BTL_REASON_BIT18   0x40000

/* Local view of the result block inside the battle work (work + 0x1938); battle/battle.h has BattleResult. */
typedef struct BtlSeqResult {
    /* 0x00 */ s32 winner;
    /* 0x04 */ s32 reason;
    /* 0x08 */ u8 unk8[0x24];
    /* 0x2C */ f32 health[2]; /* compared to pick the winner of a time up or of a double finish */
} BtlSeqResult;

extern BtlSeq *gBtlSeq;
extern BtlSeqState gBtlSeqTblDefault[BTL_SEQ_STATE_COUNT];
extern BtlSeqState gBtlSeqTblMode1[BTL_SEQ_STATE_COUNT];
extern BtlSeqState gBtlSeqTblMode5to7[BTL_SEQ_STATE_COUNT];

s32 BtlSeq_FirstState(BtlSeqState *table);
s32 BtlSeq_Call(BtlSeqFunc func, s32 state);
s32 BtlSeq_Reset(void);
s32 BtlSeq_Init(void);
void BtlSeq_Term(void);
s32 BtlSeq_PreUpdate(void);
s32 BtlSeq_Update(void);
BtlClock *BtlSeq_GetClock(void);
BtlClock *BtlSeq_GetSubClock(void);
s32 BtlSeq_GetState(void);
s32 BtlSeq_IsFighting(void);
s32 BtlSeq_IsFinish(void);
s32 BtlSeq_CallExtra(void);
void BtlSeq_SetEndCheckOff(s32 off);
s32 BtlSeq_GetEndCheckOff(void);
void BtlClock_Clear(BtlClock *clock);
s32 BtlClock_Tick(BtlClock *clock);
void BtlSeq_ResetClocks(void);
s32 BtlSeq_TickClocks(void);
void BtlSeq_ResetSubClock(void);
s32 BtlSeq_GetTimeLeft(void);
void BtlSeq_StopTalk(void);
s32 BtlSeq_CheckBattleEnd(void);

/*
 * Skill-list text of the pause menu, 0x215420-0x216AC0 (the first part of src/battle/btl_seq.c).
 * Each side has a list of pages ('$' lines of the script) holding entries ('*' lines).
 */

/* One side's list state. */
typedef struct BtlTextList {
    /* 0x00 */ s32 pages;      /* number of '$' lines */
    /* 0x04 */ s32 count[10];  /* '*' entries per page */
    /* 0x2C */ s32 page;       /* current page */
    /* 0x30 */ s32 scroll[10]; /* first visible entry per page */
    /* 0x58 */ s32 cursor[10]; /* selected entry per page */
} BtlTextList; /* size 0x80 */

/* The work behind D_002FEB30 (owned by the pause menu code before this range). */
typedef struct BtlTextWork {
    /* 0x00 */ u8 unk0[0xC];
    /* 0x0C */ s32 side;   /* which list is shown */
    /* 0x10 */ s32 unk10;  /* bit number tested against a line's mask digit */
    /* 0x14 */ s32 unk14;
    /* 0x18 */ u16 *text;  /* func_00214FE0() */
    /* 0x1C */ u16 *text2; /* func_00214FF0() */
    /* 0x20 */ BtlTextList list[2];
} BtlTextWork;

void BtlText_DrawPart(void *pkt, s32 x, s32 y, s32 w, s32 h, s32 part);
void BtlText_DrawPageIcon(void *pkt, s32 x, s32 y, u16 digit);
s32 BtlText_CheckLineMask(u16 **cursor, s32 side);
s32 BtlText_CheckUnlock(u16 **cursor);
void BtlText_DrawList(void *pkt, s32 x0, s32 x1, s32 y0, s32 y1, s32 mode);
void BtlText_DrawScrollBar(void *pkt, s32 unused, s32 x, s32 y0, s32 y1);
void BtlText_CountEntries(void);
u16 *BtlText_FindEntry(s32 n);
void BtlText_DrawEntryName(s32 x, s32 y, s32 n, s32 align, f32 alpha);

#endif
