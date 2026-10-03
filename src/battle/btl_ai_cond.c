#include "common.h"
#include "battle/btl_ai_int.h"

/*
 * CPU player, second object (its start): helpers, the rule conditions and the level-scaled rate getters,
 * 0x1B6D00..0x1B80F8. The object goes on past this file (to at least 0x1BA308).
 */

/* The object's file-scope tables, 0x2EDA70..0x2EDF08: they come before all of its function-local data, so they
 * were defined at the top of the source file. D_002EDA70 maps a rule condition id to the index of its condition
 * function; the others are column tables read by the rule evaluator further on in the object (not decompiled
 * here). Kept as assembly data until that code is. */
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDA70);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDC70);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDCE0);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDD50);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDDC0);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDDD8);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDE10);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDEB0);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_cond", D_002EDEE8);

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

/* Fighter flag 6 equals arg. Condition 34 contains an inlined copy of condition 9, so the test was an inline
 * function in the original; a non-static `inline` BtlAiCond_Flag6 itself matches too, but this compiler emits such a
 * function at the end of the object instead of here. */
static inline s32 BtlAiCond_TestFlag6(BtlAiWork *ai, u8 arg) {
    return func_0020B7C0(ai->objId) == arg;
}

/* Condition 9: fighter flag 6 equals arg. */
s32 BtlAiCond_Flag6(BtlAiWork *ai, u8 arg) {
    return BtlAiCond_TestFlag6(ai, arg);
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
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_cond", BtlAiCond_TypeRateByOppAction);

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
    if (BtlAiCond_TestFlag6(ai, 0)) {
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
RODATA_ALIGN16(); /* the jump table is at 0x2EDFA0; the data before it ends at 0x2EDF98 */
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_cond", BtlAi_GetPairRate);

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
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_cond", BtlAi_GetQuadRate);
