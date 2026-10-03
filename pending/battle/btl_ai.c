#include "common.h"
#include "battle/btl_ai.h"
#include "sys/rand.h"

/*
 * CPU player: step handlers 14..23, the per-frame sequence runner and input hand-over, the rule conditions and the
 * level-scaled rate helpers. 0x1B6008..0x1B80F8.
 *
 * Per frame (BtlAiMgr_Update, 0x1BB620, for each CPU-controlled fighter): sense (0x1BFF70), think (0x1BAC30: the
 * rule lists of the AI data, tested with the BtlAiCond_* functions below), BtlAi_RunSeq, BtlAi_SendInput.
 *
 * This range belongs to TWO original objects. The step handlers (and everything before them from 0x1B4140) are
 * one; the conditions and what follows up to at least 0x1BA308 are another: the second object's file-scope tables
 * (D_002EDA70 .. D_002EDEE8) sit in .rodata between the two groups' function-local data. The boundary is
 * somewhere in 0x1B6B08..0x1B6D00 (marked below); see the report.
 */

extern BtlAi *gBtlAi;
extern BtlAiStateFunc gBtlAiStateFuncs[4];
extern s32 D_002EDA70[]; /* rule condition id -> index of the condition function */

/* Part of the fighter data that func_00208B98 returns. */
typedef struct BtlAiChrMoves {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s16 unk10[0x47];
    /* 0x9E */ s8 unk9E[2];
} BtlAiChrMoves;

/* Part of the fighter data that func_00208B58 returns. */
typedef struct BtlAiChrSkills {
    /* 0x000 */ u8 unk0[0x13E];
    /* 0x13E */ s8 unk13E[0x27];
    /* 0x165 */ s8 unk165[0x37];
    /* 0x19C */ s32 cost[1];
} BtlAiChrSkills;

/* The action table seen from its +8 (the form step handler 17 uses). */
typedef struct BtlAiActBody {
    /* 0x000 */ u8 unk0[0x380];
    /* 0x380 */ u8 actClass[1];
} BtlAiActBody;

extern s32 BtlSeq_GetState(void);
extern s32 BtlChar_IsStage4Or27(void);
extern void BtlCharApi_SetInjectedInput(s32 objId, u32 buttons, f32 stickX, f32 stickY);
extern s32 BtlCharApi_GetUnk974(s32 objId); /* the fighter's current action id */
extern s32 func_001B5838(BtlAiWork *ai);
extern s32 func_001B5B78(BtlAiWork *ai);
extern void func_001BC918(BtlAiOutput *out, s32 keep);
extern void func_001BCD70(BtlAiOutput *out);
extern void func_001BC8A8(BtlAiWork *ai);
extern void func_001BDAB0(BtlAiWork *ai);
extern void func_001BDE68(BtlAiWork *ai);
extern void func_001BE140(BtlAiWork *ai);
extern s32 func_00206D68(s32 objId);
extern s32 func_002086C0(s32 objId, s32 arg);
extern void func_00208A28(s32 objId);
extern BtlAiChrSkills *func_00208B58(s32 objId);
extern BtlAiChrMoves *func_00208B98(s32 objId);
extern s32 func_00208550(s32 objId);
extern s32 func_00208C30(s32 objId);
extern s32 func_00208D48(s32 objId); /* member entry + 0x1C */
extern s32 func_00209378(s32 objId);
extern s32 func_002093B0(s32 objId);
extern s32 func_002095E8(s32 objId);
extern s32 func_00209670(s32 objId);
extern s32 func_002096E8(s32 objId);
extern s32 func_002098C0(s32 objId);
extern s32 func_002098E8(s32 objId);
extern s32 func_00209910(s32 objId);
extern s32 func_002099C0(s32 objId);
extern s32 func_002099E8(s32 objId);
extern s32 func_00209AC0(s32 objId);
extern f32 func_00209CE0(s32 objId);
extern s32 func_00209D98(s32 objId, s32 arg);
extern s32 func_00209E38(s32 objId);
extern s32 func_00209EA0(s32 objId, s32 arg);
extern s32 func_0020B4E0(s32 objId); /* member entry + 0xC */
extern s32 func_0020B518(s32 objId); /* member entry + 0x14 */
extern s32 func_0020B7C0(s32 objId); /* fighter flag 6 */

s32 BtlAiCond_GuardRoll(BtlAiWork *ai);
void BtlAi_ScaleByGauge(BtlAiWork *ai, s32 *lo, s32 *hi);
f32 BtlAi_GetLowGaugeFactor(BtlAiWork *ai);

/* The entry on top of the sequence stack. Written with a shift: an array index gives the other operand order. */
#define SEQ_TOP(seq) (*(BtlAiSeqEntry *)((u8 *)(seq) + ((seq)->depth << 3) + 0xC))
/* Five-step tables by level: 0..5, 6..11, 12..17, 18..23, 24..29. */
#define LEVEL_IDX(ai) ((ai)->level < 0 ? 0 : (ai)->level / 6)
#define LEVEL_STEP(ai, tbl) ((ai)->level < 0 ? 0 : (tbl)[(ai)->level / 6])
/* The character's own rate table: one signed byte per rate at level 0 (+8) and at level 29 (+0x100). */
#define RATE(ai, lo, hi, n) BtlAi_ScaleByLevel((ai)->level, lo[n], hi[n])

/* ---- Step handlers 14..23 of the table at 0x2C4708 (first object). Non-zero = step finished. ---- */

/* Step handler 14: compares a fighter value of both sides (kind 1 or 2 in seq->timer), picks one of two AI-type
 * rates for it and, on a successful roll, moves seq->step on (modulo 4). */
s32 BtlAiStep_Unk14(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    s8 cls = gBtlAi->data->act->actClass[BtlCharApi_GetUnk974(ai->objId)];
    s32 roll = Rand_Range(100);
    u8 *prof = gBtlAi->data->profile[ai->type];
    u8 *lo = prof + 0x2AC;
    u8 *hi = prof + 0x56C;
    s32 val[2];
    s32 a;
    s32 b;

    switch (seq->timer) {
    case 1:
        a = BtlAi_ScaleByLevel(ai->level, lo[4], hi[4]);
        b = BtlAi_ScaleByLevel(ai->level, lo[3], hi[3]);
        val[0] = func_002098C0(ai->objId);
        val[1] = func_002098C0(ai->objId ^ 1);
        break;
    case 2:
        a = BtlAi_ScaleByLevel(ai->level, lo[6], hi[6]);
        b = BtlAi_ScaleByLevel(ai->level, lo[5], hi[5]);
        val[0] = func_002098E8(ai->objId);
        val[1] = func_002098E8(ai->objId ^ 1);
        break;
    default:
        return 1;
    }
    if (!(val[0] > val[1])) {
        a = b;
    }
    if (roll < a) {
        seq->step = (seq->step + 1) % 4;
    }
    switch (seq->timer) {
    case 1:
        if (cls == 0x20) {
            break;
        }
        return 1;
    case 2:
        if (func_00206D68(ai->objId)) {
            break;
        }
        return 1;
    default:
        return 0;
    }
    return 0;
}

/* Step handler 15: runs func_001B5838; once that is done, checks the queued move and arms the 300-frame cooldown. */
s32 BtlAiStep_Unk15(BtlAiWork *ai) {
    BtlAiPlan *plan = &ai->plan;
    s32 busy = func_001B5838(ai);
    BtlAiChrMoves *p = func_00208B98(ai->objId);
    s32 step;
    BtlAiSeq *seq;

    seq = &ai->seq;
    step = seq->step;
    if ((u32)step >= 2) {
        return 1;
    }
    if (busy != 0) {
        return 1;
    }
    if (p->unk9E[step] == 4) {
        seq->flags |= 0x400;
    }
    switch (p->unk10[step]) {
    case 0xC:
    case 0x33:
    case 0x37:
        if (func_00209910(ai->objId) != 0) {
            return 1;
        }
        break;
    }
    plan->cooldown = 300;
    return 0;
}

/* Step handler 16: waits up to 60 frames for the opponent to come within twice range.unk4. */
s32 BtlAiStep_WaitNear2(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    BtlAiRange *range = &ai->range;

    seq->timer++;
    if (seq->timer > 60) {
        return 1;
    }
    if (range->unk4 + range->unk4 < gBtlAi->distance) {
        return 0;
    }
    return 1;
}

/* Step handler 17: waits for a skill (plan.unk8) to become affordable or usable against the opponent's action. */
#if 0
/* Best attempt. 63 of 154 instructions differ: same blocks, but the branch layout of the first half and the registers of the class comparisons at the end are off. */
s32 BtlAiStep_Unk17(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    BtlAiPlan *plan = &ai->plan;
    BtlAiChrSkills *p = func_00208B58(ai->objId);
    BtlAiActTable *act = gBtlAi->data->act;
    s32 busy = func_001B5838(ai);
    f32 rate = func_00209CE0(ai->objId);
    u32 *top = &SEQ_TOP(seq).id;
    s32 action[2];
    s8 cls[2];
    BtlAiActBody *tbl;
    s8 kind;

    if (plan->unk8 == -1) {
        return 1;
    }
    action[0] = BtlCharApi_GetUnk974(ai->objId);
    action[1] = BtlCharApi_GetUnk974(ai->objId ^ 1);
    tbl = (BtlAiActBody *)((u8 *)act + 8);
    cls[0] = tbl->actClass[action[0]];
    cls[1] = tbl->actClass[action[1]];
    if (busy == 0 && !(seq->flags & 0x1000)) {
        if (!(seq->flags & 0x40)) {
            return 0;
        }
        if (p->cost[plan->unk8] > func_0020B4E0(ai->objId)) {
            return 1;
        }
        seq->flags = (seq->flags ^ 0x800) & ~0x40;
    } else {
        seq->flags |= 0x1000;
        if (*top == 0x3F) {
            return 1;
        }
        seq->unk58 = 0;
        if (p->unk165[plan->unk8] == 5 && BtlChar_IsStage4Or27() != 0) {
            if (cls[0] != 0x16) {
                return 1;
            }
            seq->step = 3;
            if (func_00209D98(ai->objId, 1) != 0) {
                return 0;
            }
            seq->step = 2;
        } else {
            kind = p->unk13E[plan->unk8];
            if (kind != 2) {
                return 1;
            }
            if (cls[0] != 0x16) {
                return 1;
            }
            if (cls[1] == kind) {
                return 0;
            }
            if (cls[1] == 0x1C) {
                return 1;
            }
            if (cls[1] == cls[0]) {
                return 1;
            }
            if (cls[1] == 0x17) {
                return 1;
            }
            if (cls[1] == 0x19) {
                return 1;
            }
            if (cls[1] == 0x18) {
                return 1;
            }
            if ((u32)((u8)cls[1] - 0xF) < 3) {
                return 1;
            }
            if ((s32)(rate * 100.0f) < seq->timer) {
                return 0;
            }
            return 1;
        }
    }
    return 0;
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai", BtlAiStep_Unk17);

/* Step handler 18: counts seq->timer down, reloading it by level and distance, then runs func_001B5B78. */
s32 BtlAiStep_Unk18(BtlAiWork *ai) {
    s32 near[5] = { 15, 7, 5, 3, 3 };
    s32 far[5] = { 15, 10, 7, 5, 3 };
    BtlAiSeq *seq = &ai->seq;
    s32 dist = func_00208C30(ai->objId);
    s32 t = seq->timer;

    seq->step = 1;
    seq->timer = t - 1;
    if ((u32)(t - 2) >= 15) {
        if (dist < 10000) {
            seq->timer = near[LEVEL_IDX(ai)];
        } else {
            seq->timer = far[LEVEL_IDX(ai)];
        }
        seq->step = 0;
    }
    return func_001B5B78(ai);
}

/* Step handler 19: same countdown, done when func_002099C0 is not positive. */
s32 BtlAiStep_Unk19(BtlAiWork *ai) {
    s32 near[5] = { 15, 7, 5, 3, 3 };
    s32 far[5] = { 15, 10, 7, 5, 3 };
    BtlAiSeq *seq = &ai->seq;
    s32 dist = func_00208C30(ai->objId);
    s32 t = seq->timer;

    seq->step = 1;
    seq->timer = t - 1;
    if ((u32)(t - 2) >= 15) {
        if (dist < 10000) {
            seq->timer = near[LEVEL_IDX(ai)];
        } else {
            seq->timer = far[LEVEL_IDX(ai)];
        }
        seq->step = 0;
    }
    return func_002099C0(ai->objId) < 1;
}

/* Step handler 20: done when func_00209670 is zero. */
s32 BtlAiStep_Unk20(BtlAiWork *ai) {
    return func_00209670(ai->objId) == 0;
}

/* Step handler 21: same countdown, done when func_00209E38 is zero. */
s32 BtlAiStep_Unk21(BtlAiWork *ai) {
    s32 near[5] = { 15, 7, 5, 3, 3 };
    s32 far[5] = { 15, 10, 7, 5, 3 };
    BtlAiSeq *seq = &ai->seq;
    s32 dist = func_00208C30(ai->objId);
    s32 t = seq->timer;

    seq->step = 1;
    seq->timer = t - 1;
    if ((u32)(t - 2) >= 15) {
        if (dist < 10000) {
            seq->timer = near[LEVEL_IDX(ai)];
        } else {
            seq->timer = far[LEVEL_IDX(ai)];
        }
        seq->step = 0;
    }
    return func_00209E38(ai->objId) == 0;
}

/* Step handler 22: like 16 with three times the range, then func_001B5838. */
s32 BtlAiStep_WaitNear3(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    BtlAiRange *range = &ai->range;

    seq->timer++;
    if (seq->timer > 60) {
        return 1;
    }
    if (range->unk4 * 3.0f < gBtlAi->distance) {
        return 0;
    }
    return func_001B5838(ai);
}

/* Step handler 23: after func_001B5838, on stage 4 or 27, picks the next step from a skill slot lookup. */
s32 BtlAiStep_Unk23(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    s32 busy = func_001B5838(ai);
    BtlAiChrSkills *p = func_00208B58(ai->objId);
    s8 cls = gBtlAi->data->act->actClass[BtlCharApi_GetUnk974(ai->objId)];
    s32 n;

    if (busy == 0) {
        return 0;
    }
    if (BtlChar_IsStage4Or27() == 0) {
        return 1;
    }
    switch (seq->unk7C) {
    case 0x32:
        n = 7;
        break;
    case 0x2A:
        n = 4;
        break;
    case 0x24:
        n = 2;
        break;
    case 0x35:
        n = 8;
        break;
    case 0x26:
        n = 6;
        break;
    case 0x22:
        n = 1;
        break;
    case 0x2B:
        n = 5;
        break;
    case 0x29:
        n = 3;
        break;
    case 0x2D:
    case 0x2E:
        n = 0;
        break;
    default:
        return 0;
    }
    if (p->unk165[func_00209EA0(ai->objId, n)] != 5) {
        return 1;
    }
    seq->step = 0;
    if (func_00209D98(ai->objId, 1) == 0) {
        seq->step = 1;
    }
    return cls == 0;
}

/* ---- Top level. The object boundary is somewhere between here and BtlAi_ScaleByLevel. ---- */

/* Runs sequence 3: a one-shot chosen by the entry's kind byte. */
void BtlAi_RunInstant(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;

    switch (SEQ_TOP(seq).kind) {
    case 0:
        func_00208A28(ai->objId);
        break;
    case 1:
        seq->flags |= 0x80;
        break;
    case 2:
        BtlAi_NoteOpponent(ai, 0x20000);
        break;
    }
}

/* Once per frame: clears the output, runs the sequence on top of the stack, then finishes the output. */
void BtlAi_RunSeq(BtlAiWork *ai) {
    BtlAiOutput *out = &ai->out;
    BtlAiSeq *seq = &ai->seq;

    func_001BC918(out, 1);
    if (seq->depth > 0) {
        switch (SEQ_TOP(seq).id) {
        case 3:
            BtlAi_RunInstant(ai);
            seq->depth--;
            break;
        case 7:
            func_001BC8A8(ai);
            break;
        case 0x19:
            func_001BDAB0(ai);
            break;
        case 0x1A:
            func_001BDE68(ai);
            break;
        case 0x36:
            func_001BE140(ai);
            break;
        default:
            gBtlAiStateFuncs[seq->phase](ai);
            break;
        }
        func_001BCD70(out);
    }
}

/* Hands this frame's buttons and stick to the fighter (the same path a pad feeds). */
void BtlAi_SendInput(BtlAiWork *ai) {
    BtlCharApi_SetInjectedInput(ai->objId, ai->out.buttons, ai->out.stickX, ai->out.stickY);
}

/* ---- Second object: helpers, rule conditions (table at 0x2C4768, same order), rate getters. ---- */

/* Interpolates between lo (level 0) and hi (level 29). */
s32 BtlAi_ScaleByLevel(s32 level, s32 lo, s32 hi) {
    if (level < 0) {
        level = 0;
    }
    return lo + (s32)((f32)(hi - lo) / 29.0f * (f32)level + 0.01f);
}

/* Records the opponent's current action and its class, and raises reaction bits. */
void BtlAi_NoteOpponent(BtlAiWork *ai, s32 react) {
    BtlAiActTable *act = gBtlAi->data->act;
    s32 action = BtlCharApi_GetUnk974(ai->objId ^ 1);
    BtlAiStatus *st = &ai->status;
    s32 cls = act->actClass[action];

    st->oppAction = action;
    st->oppClass = cls;
    st->react |= react;
}

/* Condition 0: never. */
s32 BtlAiCond_False(BtlAiWork *ai, u8 arg) {
    return 0;
}

/* Condition 1: always. */
s32 BtlAiCond_True(BtlAiWork *ai, u8 arg) {
    return 1;
}

/* Condition 2: arg percent chance. */
s32 BtlAiCond_Percent(BtlAiWork *ai, u8 arg) {
    BtlAiSeq *seq = &ai->seq;
    s32 roll = Rand_Range(100);

    seq->roll = roll;
    seq->threshold = arg;
    return roll < arg;
}

/* Condition 3: no sequence is running. */
s32 BtlAiCond_Idle(BtlAiWork *ai, u8 arg) {
    return ai->seq.depth == 0;
}

/* Condition 4: level >= arg. */
s32 BtlAiCond_LevelAtLeast(BtlAiWork *ai, u8 arg) {
    if (ai->level < arg) {
        return 0;
    }
    return 1;
}

/* Condition 5: level < arg. */
s32 BtlAiCond_LevelBelow(BtlAiWork *ai, u8 arg) {
    return ai->level < arg;
}

/* Condition 6: status bit 4 equals arg. */
s32 BtlAiCond_Status4(BtlAiWork *ai, u8 arg) {
    return ((s32)(ai->status.flags >> 4) & 1) == arg;
}

/* Condition 7: member word +0xC (of 100000) is at least arg percent. */
s32 BtlAiCond_GaugeAPercent(BtlAiWork *ai, u8 arg) {
    if ((f32)func_0020B4E0(ai->objId) / 100000.0f * 100.0f >= (f32)arg) {
        return 1;
    }
    return 0;
}

/* Condition 8: member word +0x14 holds at least arg units of 100000. */
s32 BtlAiCond_GaugeBStock(BtlAiWork *ai, u8 arg) {
    if (func_0020B518(ai->objId) / 100000 < arg) {
        return 0;
    }
    return 1;
}

/* Condition 9: fighter flag 6 equals arg. Inline: condition 34 contains a copy. */
inline s32 BtlAiCond_Flag6(BtlAiWork *ai, u8 arg) {
    return func_0020B7C0(ai->objId) == arg;
}

/* Condition 10: status bit 6 equals arg. */
s32 BtlAiCond_Status6(BtlAiWork *ai, u8 arg) {
    return ((s32)(ai->status.flags >> 6) & 1) == arg;
}

/* Condition 11: on stage 4 or 27 and func_00209D98(1) is zero. */
s32 BtlAiCond_Unk11(BtlAiWork *ai, u8 arg) {
    s32 r;

    if (BtlChar_IsStage4Or27() == 0) {
        r = 0;
    } else {
        r = func_00209D98(ai->objId, 1) == 0;
    }
    return r == arg;
}

/* Condition 12: func_002096E8 equals arg. */
s32 BtlAiCond_Unk12(BtlAiWork *ai, u8 arg) {
    return func_002096E8(ai->objId) == arg;
}

/* Condition 13: plan word 8 is 2. */
s32 BtlAiCond_Plan8Is2(BtlAiWork *ai, u8 arg) {
    return (ai->plan.unk8 == 2) == arg;
}

/* Conditions 14, 15, 16: bits 0, 1, 2 of work + 0xC. */
s32 BtlAiCond_WorkBit0(BtlAiWork *ai, u8 arg) {
    return (ai->unkC & 1) == arg;
}

s32 BtlAiCond_WorkBit1(BtlAiWork *ai, u8 arg) {
    return ((ai->unkC >> 1) & 1) == arg;
}

s32 BtlAiCond_WorkBit2(BtlAiWork *ai, u8 arg) {
    return ((ai->unkC >> 2) & 1) == arg;
}

/* Condition 17: not a test, stores arg in the plan and passes. */
s32 BtlAiCond_SetPlan54(BtlAiWork *ai, u8 arg) {
    ai->plan.unk54 = arg;
    return 1;
}

/* Condition 18: flag bit 2 of the running sequence equals arg (fails when idle). */
s32 BtlAiCond_SeqFlag2(BtlAiWork *ai, u8 arg) {
    BtlAi *mgr = gBtlAi;
    BtlAiSeq *seq = &ai->seq;
    BtlAiActTable *act = mgr->data->act;

    if (seq->depth == 0) {
        return 0;
    }
    return ((act->seqFlags[SEQ_TOP(seq).id] >> 2) & 1) == arg;
}

/* Condition 19: level below -1. */
s32 BtlAiCond_LevelBelowDummy(BtlAiWork *ai, u8 arg) {
    return (ai->level < -1) == arg;
}

/* Helper of condition 20: 75% chance to react to an opponent action of class 1..7, 22 or 23. */
s32 BtlAiCond_GuardRoll(BtlAiWork *ai) {
    BtlAiStatus *st = &ai->status;
    s8 cls = gBtlAi->data->act->actClass[BtlCharApi_GetUnk974(ai->objId ^ 1)];
    s32 roll = Rand_Range(100);

    if (!((u32)(cls - 1) < 7) && cls != 0x16 && cls != 0x17) {
        return 0;
    }
    if (st->flags & 0x1000000) {
        return 0;
    }
    if (roll < 25) {
        return 0;
    }
    st->react &= ~0x10000;
    return 1;
}

/* Condition 20: reaction rolls against what the opponent is doing; the chance grows with the level. */
s32 BtlAiCond_React(BtlAiWork *ai, u8 arg) {
    s32 guard[5] = { 20, 40, 60, 80, 95 };
    s32 evade[5] = { 10, 20, 30, 40, 50 };
    s32 blastA[5] = { 0, 4, 8, 12, 15 };
    s32 blastB[5] = { 0, 3, 5, 8, 10 };
    s32 blastC[5] = { 0, 2, 3, 4, 5 };
    s32 step = LEVEL_IDX(ai);
    s32 roll = Rand_Range(100);
    s32 action = BtlCharApi_GetUnk974(ai->objId ^ 1);
    s32 chance;

    switch (arg) {
    case 0:
        BtlAi_NoteOpponent(ai, 0x10000);
        if (func_002099E8(ai->objId) & 4) {
            return BtlAiCond_GuardRoll(ai);
        }
    case 1:
        chance = LEVEL_STEP(ai, guard);
        break;
    case 2:
        BtlAi_NoteOpponent(ai, 0x80000);
        chance = LEVEL_STEP(ai, evade);
        break;
    case 3:
        BtlAi_NoteOpponent(ai, 0x100000);
        if (action == 0x37 || action == 0x3C) {
            chance = LEVEL_STEP(ai, blastA);
        } else if (action == 0x38 || action == 0x3D) {
            chance = LEVEL_STEP(ai, blastB);
        } else if (action == 0x39 || action == 0x3E) {
            chance = LEVEL_STEP(ai, blastC);
        } else {
            BtlAi_NoteOpponent(ai, 0x10);
            return 0;
        }
        break;
    default:
        return 0;
    }
    return roll < chance;
}

/* Condition 21: the battle sequence is in the Ready state. */
s32 BtlAiCond_SeqReady(BtlAiWork *ai, u8 arg) {
    return (BtlSeq_GetState() == 2) == arg;
}

/* Condition 22: the stage is 4 or 27. */
s32 BtlAiCond_Stage4Or27(BtlAiWork *ai, u8 arg) {
    return BtlChar_IsStage4Or27() == arg;
}

/* Condition 23: once per reaction bit 0x40000, a level-stepped chance. */
s32 BtlAiCond_React40000(BtlAiWork *ai, u8 arg) {
    s32 tbl[5] = { 40, 55, 70, 85, 95 };
    s32 chance = LEVEL_STEP(ai, tbl);
    s32 roll = Rand_Range(100);

    if (ai->status.react & 0x40000) {
        return 0;
    }
    BtlAi_NoteOpponent(ai, 0x40000);
    return roll < chance;
}

/* Condition 24: func_00208550 equals arg. */
s32 BtlAiCond_Unk24(BtlAiWork *ai, u8 arg) {
    return func_00208550(ai->objId) == arg;
}

/* Condition 25: character rate 0. */
s32 BtlAiCond_Rate0(BtlAiWork *ai, u8 arg) {
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 0);
    BtlAiSeq *seq = &ai->seq;

    seq->roll = roll;
    seq->threshold = chance;
    if (ai->status.react & 2) {
        BtlAi_NoteOpponent(ai, 0x20);
    }
    return roll < chance;
}

/* Condition 26: character rate 5, only while one of two fighter flags is up. */
s32 BtlAiCond_Rate5(BtlAiWork *ai, u8 arg) {
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 5);
    s32 a = func_00209378(ai->objId);
    s32 b = func_002093B0(ai->objId);

    if (!(a & 0x20) && !(b & 2)) {
        return 0;
    }
    return roll < chance;
}

/* Condition 27: func_002086C0(1) is 2 (only with arg 1). */
s32 BtlAiCond_Unk27(BtlAiWork *ai, u8 arg) {
    if (func_002086C0(ai->objId, 1) == 2 && arg == 1) {
        return 1;
    }
    return 0;
}

/* Condition 28: character rate 6; raises reaction bit 0x80. */
s32 BtlAiCond_Rate6(BtlAiWork *ai, u8 arg) {
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 6);

    BtlAi_NoteOpponent(ai, 0x80);
    return roll < chance;
}

/* Condition 29: character rate 1; raises reaction bit 0x40 when a fighter query returns -1. */
s32 BtlAiCond_Rate1(BtlAiWork *ai, u8 arg) {
    BtlAiSeq *seq = &ai->seq;
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 1);

    if (func_002095E8(ai->objId) == -1) {
        BtlAi_NoteOpponent(ai, 0x40);
    }
    seq->roll = roll;
    seq->threshold = chance;
    return roll < chance;
}

/* Condition 30: character rate 4; raises reaction bit 0x100. */
s32 BtlAiCond_Rate4(BtlAiWork *ai, u8 arg) {
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 4);

    BtlAi_NoteOpponent(ai, 0x100);
    return roll < chance;
}

/* Condition 31: AI-type rate chosen by which of the opponent actions 0x37..0x3A / 0x3C..0x3F is running. */
#if 0
/* Best attempt. 5 of 50 instructions differ: the range test result goes to v1 instead of s0, and the two pointer adds come after it. */
s32 BtlAiCond_TypeRateByOppAction(BtlAiWork *ai, u8 arg) {
    BtlAiSeq *seq = &ai->seq;
    s32 action = BtlCharApi_GetUnk974(ai->objId ^ 1);
    u8 *prof = gBtlAi->data->profile[ai->type];
    s8 *lo = (s8 *)prof + 0x2A8;
    s8 *hi = (s8 *)prof + 0x568;
    s32 idx = action < 0x3C ? action - 0x37 : action - 0x3C;
    s32 roll = Rand_Range(100);
    s32 chance;
    s8 *l = lo + idx;
    s8 *h = hi + idx;

    if ((u32)idx >= 4) {
        return 0;
    }
    chance = BtlAi_ScaleByLevel(ai->level, *l, *h);
    seq->roll = roll;
    seq->threshold = chance;
    return roll < chance;
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai", BtlAiCond_TypeRateByOppAction);

/* Condition 32: character rate 2; raises reaction bit 0x400. */
s32 BtlAiCond_Rate2(BtlAiWork *ai, u8 arg) {
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 2);
    BtlAiSeq *seq = &ai->seq;
    BtlAiStatus *st;

    seq->roll = roll;
    seq->threshold = chance;
    st = &ai->status;
    st->react |= 0x400;
    return roll < chance;
}

/* Condition 33: character rate 8 (+20 for arg 1), or a fighter query; arms a 90-frame timer. */
s32 BtlAiCond_Rate8(BtlAiWork *ai, u8 arg) {
    BtlAiStatus *st = &ai->status;
    BtlAiSeq *seq = &ai->seq;
    s8 *lo = (s8 *)ai->param + 8;
    s8 *hi = (s8 *)ai->param + 0x100;
    s32 roll = Rand_Range(100);
    s32 chance = RATE(ai, lo, hi, 8);
    s32 force = func_00209AC0(ai->objId);

    if (arg == 0) {
        st->timer18 = 90;
    } else if (arg == 1) {
        st->timer1C = 90;
        chance += 20;
    }
    seq->roll = roll;
    seq->threshold = chance;
    if (roll < chance || force) {
        return 1;
    }
    return 0;
}

/* Condition 34: 10% chance, never while fighter flag 6 is set; arms a 90-frame timer. */
s32 BtlAiCond_TenPercent(BtlAiWork *ai, u8 arg) {
    s32 roll = Rand_Range(100);

    ai->status.timer20 = 90;
    if (BtlAiCond_Flag6(ai, 0)) {
        return roll < 10;
    }
    return 0;
}

/* Scales a lo/hi pair down as member word +0x1C (of 30000) rises above a level-dependent percentage. */
void BtlAi_ScaleByGauge(BtlAiWork *ai, s32 *lo, s32 *hi) {
    u8 *prof = gBtlAi->data->profile[ai->type];
    f32 limit = (f32)BtlAi_ScaleByLevel(ai->level, prof[0x2AC], prof[0x56C]);
    f32 pct = (f32)func_00208D48(ai->objId) / 30000.0f * 100.0f;

    s32 a = *lo;
    s32 b = *hi;

    if (limit < pct) {
        f32 k = 1.0f - (pct - limit) / (100.0f - limit);

        *lo = (s32)((f32)a * k);
        *hi = (s32)((f32)b * k);
    }
}

/* Multiplier applied when member word +0xC is under 20% / 40% of 100000. */
f32 BtlAi_GetLowGaugeFactor(BtlAiWork *ai) {
    f32 pct = (f32)func_0020B4E0(ai->objId) / 100000.0f * 100.0f;
    u8 *prof = gBtlAi->data->profile[ai->type];
    f32 r = 1.0f;
    s32 lo;
    s32 hi;

    if (pct < 20.0f) {
        lo = prof[0x2B7];
        hi = prof[0x577];
    } else if (pct < 40.0f) {
        lo = prof[0x2B6];
        hi = prof[0x576];
    } else {
        return r;
    }
    return (f32)BtlAi_ScaleByLevel(ai->level, lo, hi) * 0.1f;
}

/* Picks one of 14 lo/hi byte pairs (rows of 16, columns 0 and 8), scales them by gauge and returns the value for
 * the level. */
#if 0
/* Best attempt. The original keeps a separate copy of the row code in every even case and shares one tail for the odd ones; this collapses differently (75 of 90 instructions differ). */
s32 BtlAi_GetPairRate(BtlAiWork *ai, u32 kind, s32 off, s8 *base_lo, s8 *base_hi, s32 byGauge) {
    BtlAiPlan *plan = &ai->plan;
    s32 lo = 0;
    s32 hi = 0;
    s32 i;

    switch (kind) {
    case 0:
        i = off;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 1:
        i = off;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 2:
        i = off + 0x10;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 3:
        i = off + 0x10;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 4:
        i = off + 0x20;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 5:
        i = off + 0x20;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 6:
        i = off + 0x30;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 7:
        i = off + 0x30;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 8:
        i = off + 0x40;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 9:
        i = off + 0x40;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 10:
        i = off + 0x50;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 11:
        i = off + 0x50;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 12:
        i = off + 0x60;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 13:
        i = off + 0x60;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    default:
        return 0;
    }
    if (byGauge == 1) {
        BtlAi_ScaleByGauge(ai, &lo, &hi);
    }
    if (D_002EDA70[plan->cond] == 0x26) {
        f32 k = BtlAi_GetLowGaugeFactor(ai);

        lo = (s32)((f32)lo * k);
        hi = (s32)((f32)hi * k);
    }
    return BtlAi_ScaleByLevel(ai->level, lo, hi);
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai", BtlAi_GetPairRate);

/* Same with rows of four pairs (columns 0, 4, 8, 12) and no low-gauge factor. */
#if 0
/* Best attempt. Same problem as BtlAi_GetPairRate. */
s32 BtlAi_GetQuadRate(BtlAiWork *ai, u32 kind, s32 off, s8 *base_lo, s8 *base_hi, s32 byGauge) {
    s32 lo = 0;
    s32 hi = 0;
    s32 i;

    switch (kind) {
    case 0:
        i = off;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 1:
        i = off;
        lo = base_lo[i + 4];
        hi = base_hi[i + 4];
        break;
    case 2:
        i = off;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 3:
        i = off;
        lo = base_lo[i + 12];
        hi = base_hi[i + 12];
        break;
    case 4:
        i = off + 0x10;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 5:
        i = off + 0x10;
        lo = base_lo[i + 4];
        hi = base_hi[i + 4];
        break;
    case 6:
        i = off + 0x10;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 7:
        i = off + 0x10;
        lo = base_lo[i + 12];
        hi = base_hi[i + 12];
        break;
    case 8:
        i = off + 0x20;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 9:
        i = off + 0x20;
        lo = base_lo[i + 4];
        hi = base_hi[i + 4];
        break;
    case 10:
        i = off + 0x20;
        lo = base_lo[i + 8];
        hi = base_hi[i + 8];
        break;
    case 11:
        i = off + 0x20;
        lo = base_lo[i + 12];
        hi = base_hi[i + 12];
        break;
    case 12:
        i = off + 0x30;
        lo = base_lo[i];
        hi = base_hi[i];
        break;
    case 13:
        i = off + 0x30;
        lo = base_lo[i + 4];
        hi = base_hi[i + 4];
        break;
    default:
        return 0;
    }
    if (byGauge == 1) {
        BtlAi_ScaleByGauge(ai, &lo, &hi);
    }
    return BtlAi_ScaleByLevel(ai->level, lo, hi);
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai", BtlAi_GetQuadRate);
