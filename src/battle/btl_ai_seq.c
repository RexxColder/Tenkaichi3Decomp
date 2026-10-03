#include "common.h"
#include "battle/btl_ai_int.h"

/*
 * CPU player, first object: 0x1B4140..0x1B6D50. Decompiled so far: step handlers 14..23, the per-frame sequence
 * runner and the input hand-over (0x1B6008..0x1B6D00). The rule conditions that follow are btl_ai_cond.c.
 *
 * Per frame (BtlAiMgr_Update, 0x1BB620, for each CPU-controlled fighter): sense (0x1BFF70), think (0x1BAC30: the
 * rule lists of the AI data, tested with the BtlAiCond_* functions), BtlAi_RunSeq, BtlAi_SendInput.
 *
 * Why the file starts at 0x1B4140 and not at the first decompiled function: jump tables are 16-byte aligned
 * inside an object's .rodata, so the step handlers' data (0x2ED988..0x2EDA70) only lands on its original
 * addresses when the object's .rodata starts where the original object's did. Its first item is the jump table
 * of func_001B4300 at 0x2ED8C0. The functions before 0x1B6008 are therefore pulled in from assembly.
 * Where the object ends: see the note at the top of btl_ai_cond.c.
 */

/* ---- Not decompiled yet: 0x1B4140..0x1B6008. ---- */

INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4140);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4220);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4300);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B44A8);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4608);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4830);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4A50);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4AF0);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4C00);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4C18);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4C28);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4CE8);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4E38);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4E90);
INCLUDE_RODATA("asm/nonmatchings/battle/btl_ai_seq", D_002ED970); /* func_001B4F20's table */
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B4F20);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5040);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B51D0);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B51F8);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5220);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5490);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B54B0);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5510);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5568);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5828);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5838);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5940);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5A18);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5AA8);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5AE8);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5B38);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5B78);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5BC0);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5C80);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5DA8);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5E48);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5E88);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5EB0);
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", func_001B5FD0);

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

/* Step handler 16: waits up to 60 frames for the opponent to come within twice move.dist[1]. */
s32 BtlAiStep_WaitNear2(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    BtlAiMoveWork *move = &ai->move;

    seq->timer++;
    if (seq->timer > 60) {
        return 1;
    }
    if (move->dist[1] + move->dist[1] < gBtlAi->dist) {
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
INCLUDE_ASM("asm/nonmatchings/battle/btl_ai_seq", BtlAiStep_Unk17);

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
    BtlAiMoveWork *move = &ai->move;

    seq->timer++;
    if (seq->timer > 60) {
        return 1;
    }
    if (move->dist[1] * 3.0f < gBtlAi->dist) {
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

    switch (SEQ_TOP(seq).arg) {
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

    BtlAiPad_Clear(out, 1);
    if (seq->depth > 0) {
        switch (SEQ_TOP(seq).id) {
        case 3:
            BtlAi_RunInstant(ai);
            seq->depth--;
            break;
        case 7:
            BtlAiMove_Dispatch(ai);
            break;
        case 0x19:
            BtlAiCombo_Dispatch(ai);
            break;
        case 0x1A:
            BtlAiFollow_Dispatch(ai);
            break;
        case 0x36:
            BtlAiAct36_Dispatch(ai);
            break;
        default:
            gBtlAiStateFuncs[seq->phase](ai);
            break;
        }
        BtlAiPad_EndFrame(out);
    }
}

/* Hands this frame's buttons and stick to the fighter (the same path a pad feeds). */
void BtlAi_SendInput(BtlAiWork *ai) {
    BtlCharApi_SetInjectedInput(ai->objId, ai->out.buttons, ai->out.stickX, ai->out.stickY);
}

/* Interpolates between lo (level 0) and hi (level 29). Last function of this object: its callers in the next one
 * (btl_ai_cond.c) only compile to the original bytes when it is NOT defined in their translation unit. */
s32 BtlAi_ScaleByLevel(s32 level, s32 lo, s32 hi) {
    if (level < 0) {
        level = 0;
    }
    return lo + (s32)((f32)(hi - lo) / 29.0f * (f32)level + 0.01f);
}
