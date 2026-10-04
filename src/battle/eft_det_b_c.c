/*
 * Head of the AI sequence object: 0x1B3F78..0x1B4140 (btl_ai_seq.c continues at 0x1B4140; these two functions
 * are the first of that object: its rodata starts with the table D_002ED8A0 used here).
 *
 * Not stage code. Neither function draws a random number or reads anything but the AI block.
 */
#include "common.h"
#include "battle/eft_det_b.h"

/* Clears the sequence runner: empty stack, every entry {1, 0}. */
void BtlAiSeq_Reset(DetAiSeq *seq) {
    s32 i;

    seq->flags = 0;
    seq->depth = 0;
    seq->phase = 0;
    seq->stepCount = 0;
    seq->step = 0;
    for (i = 0; i < 8; i++) {
        seq->stack[i].id = 1;
        seq->stack[i].arg = 0;
    }
    seq->skip = 0;
}

/* Replaces the sequence stack with the actions of a rule (up to four, 0xFF ends the list). Action 2 with the
   argument byte 0x80 is dropped while the skip counter is positive; the counter goes down by one per call and is
   reloaded from a table by cpu level / 6 (2, 3, 4, 5, 99999) when it runs out. Action 0x3D arms a 90-frame timer
   in the plan block. */
/* NON-MATCHING: the original does not reduce the two byte lists to walking pointers (it indexes rule + 4 and
   rule + 8 with i + 0x10 and keeps a pointer to the stack entry); 66 of 85 instructions differ. Read from the
   disassembly, the behaviour is what this C does. */
#if 0
void BtlAiSeq_PushRule(DetAiWork *ai, DetAiRule *rule) {
    DetAiSeq *seq = &ai->seq;
    s32 skips[5] = { 2, 3, 4, 5, 99999 };
    DetAiPlan *plan = (DetAiPlan *)ai->plan;
    DetAiSeqEntry *entry;
    u8 *ids;
    u8 *args;
    s32 i;

    seq->depth = 0;
    seq->phase = 0;
    ai->pathCount = 0;
    seq->skip--;
    if (seq->skip < 0) {
        seq->skip = skips[ai->level >= 0 ? ai->level / 6 : 0];
    }
    ids = (u8 *)rule + 4;
    args = (u8 *)rule + 8;
    entry = &seq->stack[seq->depth];
    for (i = 0; ids[i + 0x10] != 0xFF; entry = &seq->stack[seq->depth]) {
        if (ids[i + 0x10] != 2 || seq->skip <= 0 || args[i + 0x10] != 0x80) {
            if (ids[i + 0x10] == 0x3D) {
                plan->timerA4 = 90;
            }
            entry->id = ids[i + 0x10];
            entry->arg = args[i + 0x10] + 0x81;
            seq->depth++;
        }
        i++;
        if (i >= 4) {
            break;
        }
    }
}
#endif
INCLUDE_RODATA("asm/nonmatchings/battle/eft_det_b_c", D_002ED8A0); /* the skip table { 2, 3, 4, 5, 99999 } */
INCLUDE_ASM("asm/nonmatchings/battle/eft_det_b_c", BtlAiSeq_PushRule);
