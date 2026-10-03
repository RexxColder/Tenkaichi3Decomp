#include "common.h"
#include "sys/option.h"
#include "sys/heap.h"
#include "sys/rand.h"
#include "sys/common.h"

extern void *memset(void *dst, s32 value, u32 size);

extern void func_0025DF30(void); /* clears the per-session parts of gProgress */

/* Debug (no callers): unlocks every character, stage, list entry and item, marks about half the items new, and gives 4,850,000 money. */
void Option_UnlockAll(OptionBlock *opt) {
    OptionSlot *slot = opt->slot;
    s32 i;
    s32 *v;

    for (i = 0; i < OPTION_CHARA_WORDS; i++) {
        opt->charaBits[i] = 0;
    }
    for (i = 0; i < OPTION_CHARA_COUNT; i++) {
        OPTION_CHARA_WORD(gOption, i) |= OPTION_CHARA_MASK(i);
    }
    gOption->stageBits = 0;
    for (i = 0; i < OPTION_STAGE_COUNT; i++) {
        gOption->stageBits |= 1L << i;
    }
    for (i = 0; i < OPTION_BGM_LIST_COUNT; i++) {
        if (i >= OPTION_BGM_COUNT) {
            break;
        }
        gOption->bgmBits |= 1 << i;
    }
    for (i = 0; i < OPTION_FLAG8_COUNT; i++) {
        gOption->unlockFlags |= 1 << i;
    }
    for (i = 0; i < OPTION_ITEM_COUNT; i++) {
        opt->item[i] |= OPTION_ITEM_OWNED;
        if (Rand_Range(100) & 1) {
            opt->item[i] |= OPTION_ITEM_NEW;
        }
    }
    opt->money = OPTION_MONEY_UNLOCK_ALL;
    for (i = 0; i < OPTION_REC_COUNT; i++) {
        opt->rec[i].chara = -1;
    }
    opt->unk20C = 20;
    opt->unk208 |= 1;
    for (i = 0; i < OPTION_SLOT_COUNT; i++) {
        slot[i].flags |= 3;
        v = opt->slot[i].val;
        v[0] = 0xFFFF;
        v[1] = 0xFFFF;
        v[2] = 0xFFFF;
    }
}

/* Clears the block and writes a new save: starting characters/stages/items, default key assignment, volumes and flags. */
void Option_SetDefaults(OptionBlock *opt) {
    s32 i;
    ItemInfo *info;

    info = (ItemInfo *)((u8 *)gCommonRes->data[2] + ((ItemFile *)gCommonRes->data[2])->itemOffset / 4 * 4);
    func_0025DF30();
    memset(opt, 0, OPTION_SIZE);
    opt->slot[0].flags |= 3;
    opt->slot[0].val[0] |= 1;
    opt->slot[0].val[2] |= 1;
    opt->unkC = 1;
    Option_ResetRules();
    for (i = 0; i < OPTION_CHARA_COUNT; i++) {
        switch (i) {
        case 7:
        case 8:
        case 9:
        case 10:
        case 0x18:
        case 0x26:
        case 0x36:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x4B:
        case 0x4C:
        case 0x61:
        case 0x6E:
        case 0x78:
        case 0x91:
        case 0x95:
        case 0x96:
        case 0x97:
        case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9B:
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F:
        case 0xA0:
            break;
        default:
            OPTION_CHARA_WORD(gOption, i) |= OPTION_CHARA_MASK(i);
            break;
        }
    }
    for (i = 0; i < OPTION_STAGE_COUNT; i++) {
        switch (i) {
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x1F:
        case 0x20:
            break;
        default:
            gOption->stageBits |= 1L << i;
            break;
        }
    }
    for (i = 0; i < OPTION_BGM_LIST_COUNT; i++) {
        if (i >= OPTION_BGM_COUNT) {
            break;
        }
        gOption->bgmBits |= 1 << i;
    }
    for (i = 0; i < OPTION_ITEM_COUNT; i++) {
        if (info[i].flags & ITEM_INFO_INITIAL) {
            Option_AddItem(i);
        }
    }
    for (i = 0; i < OPTION_REC_COUNT; i++) {
        opt->rec[i].chara = -1;
    }
    opt->unk20C = 5;
    opt->unk77C = 99;
    for (i = 0; i < OPTION_PAD_COUNT; i++) {
        opt->key[i][0] = 2;
        opt->key[i][1] = 1;
        opt->key[i][2] = 0;
        opt->key[i][3] = 3;
        opt->key[i][4] = 4;
        opt->key[i][5] = 5;
        opt->key[i][6] = 6;
        opt->key[i][7] = 7;
        opt->keyEdit[i][0] = 2;
        opt->keyEdit[i][1] = 1;
        opt->keyEdit[i][2] = 0;
        opt->keyEdit[i][3] = 3;
        opt->keyEdit[i][4] = 4;
        opt->keyEdit[i][5] = 5;
        opt->keyEdit[i][6] = 6;
        opt->keyEdit[i][7] = 7;
    }
    opt->flags |= OPTION_FLAG_DEFAULT;
    opt->unk1694 = 0;
    opt->bgmVolume = OPTION_VOLUME_DEFAULT;
    opt->seVolume = OPTION_VOLUME_DEFAULT;
}

/* Empty; called with the block right after the defaults are written. */
void Option_Stub266600(OptionBlock *opt) {
}

/* Allocates the 0x4000-byte save block and fills it with a new save. */
void Option_Init(void) {
    gOption = Heap_Alloc(OPTION_SIZE, 0x20, 0, 2);
    memset(gOption, 0, OPTION_SIZE);
    Option_SetDefaults(gOption);
    Option_Stub266600(gOption);
}

/* Gives item idx (0-based) if it is not owned yet, marking it new. */
void Option_AddItem(s32 idx) {
    if (!(gOption->item[idx] & OPTION_ITEM_OWNED)) {
        gOption->item[idx] |= OPTION_ITEM_OWNED;
        gOption->item[idx] |= OPTION_ITEM_NEW;
    }
}

/* Adds amount (may be negative) to the money, clamped to 0..9,999,999. */
void Option_AddMoney(s32 amount) {
    gOption->money += amount;
    if (gOption->money > OPTION_MONEY_MAX) {
        gOption->money = OPTION_MONEY_MAX;
    } else if (gOption->money < 0) {
        gOption->money = 0;
    }
}

/* Restores the six rule settings at 0xC34 to 3, 2, 2, 0, 0, 0. */
void Option_ResetRules(void) {
    gOption->rule[0] = 3;
    gOption->rule[1] = 2;
    gOption->rule[2] = 2;
    gOption->rule[3] = 0;
    gOption->rule[4] = 0;
    gOption->rule[5] = 0;
}
