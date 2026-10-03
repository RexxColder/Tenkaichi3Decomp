#ifndef SYS_OPTION_H
#define SYS_OPTION_H

#include "types.h"

/*
 * gOption: the 0x4000-byte block allocated by Option_Init. It is the whole persistent save: the memory
 * card code copies 0x4000 bytes straight into it after a load (func_0011ABD0 / func_0011B1E0, from the
 * card buffer + 0x38) and hands it to the writer with size 0x4000 (func_00119FB8 / func_0011BA08).
 * "How we know" is given per field; "menu" means the DBZP.BIN overlay (asm/dbzp/000000.s).
 */

#define OPTION_SIZE 0x4000

#define OPTION_CHARA_COUNT 161  /* bits in charaBits (ids 0..160; 161..164 are list markers in func_0025F610) */
#define OPTION_CHARA_WORDS 3
#define OPTION_STAGE_COUNT 35   /* bits in stageBits */
#define OPTION_BGM_LIST_COUNT 25 /* entries of the list bgmBits belongs to (loop bound) */
#define OPTION_BGM_COUNT 20     /* entries of that list that have a bit */
#define OPTION_FLAG8_COUNT 7    /* bits of unlockFlags set by Option_UnlockAll */
#define OPTION_ITEM_COUNT 350   /* entries of item[] and of the item table in common file 4 */
#define OPTION_SLOT_COUNT 9
#define OPTION_CUSTOM_COUNT 97
#define OPTION_CUSTOM_SETS 3
#define OPTION_CUSTOM_ITEMS 8   /* 7 usable item slots + a zero terminator */
#define OPTION_REC_COUNT 14
#define OPTION_PAD_COUNT 2
#define OPTION_KEY_COUNT 8
#define OPTION_RULE_COUNT 6

#define OPTION_MONEY_MAX 9999999
#define OPTION_MONEY_UNLOCK_ALL 4850000
#define OPTION_VOLUME_DEFAULT 9 /* volumes are 0..9 */

/* OptionBlock.flags (0x1608). Defaults: bits 0-2 set. */
#define OPTION_FLAG_VOICE 1        /* voice set: set = file base 0xCC32, clear = 0x8D4E (sys/adx.c). First choice of the menu's voice item sets it. */
#define OPTION_FLAG_PAD_A(pad) (2 << (pad)) /* bits 1-2, one per controller; battle reads `flags & (2 << player)` (func_001C02C8). Vibration: guess. */
#define OPTION_FLAG_PAD_B(pad) (8 << (pad)) /* bits 3-4, one per controller; toggled and reset (cleared) only by the menu's controller page */
#define OPTION_FLAG_DEFAULT 7

/* OptionBlock.item[] bits. */
#define OPTION_ITEM_OWNED 1
#define OPTION_ITEM_NEW 2 /* set together with OWNED when the item is first obtained: guess for "not looked at yet" */

/* Flag of an ItemInfo entry: the item is owned in a new save. */
#define ITEM_INFO_INITIAL 2

/* 0x10 bytes; nine of them at 0x10. Indexed with `gOption + 0x10 + n * 0x10` all over the menu overlay. */
typedef struct OptionSlot {
    /* 0x00 */ s32 flags;  /* bits 0-1 set by Option_UnlockAll and, for slot 0, by the defaults; bit 2 tested/set by menu func_0033ACB0 */
    /* 0x04 */ s32 val[3]; /* 0xFFFF each after Option_UnlockAll; slot 0 gets val[0] |= 1 and val[2] |= 1 by default */
} OptionSlot;

/* 0x38 bytes; per-character customisation (menu functions 0x398A60..0x3991D8, main func_00260DB8 / func_00260E28). */
typedef struct OptionCustom {
    /* 0x00 */ u16 item[OPTION_CUSTOM_SETS][OPTION_CUSTOM_ITEMS]; /* equipped item ids (1-based, 0 = empty) of each of the 3 sets */
    /* 0x30 */ s32 unk30;  /* running total compared with the table value picked by `level` (func_00398AC8): experience, guess */
    /* 0x34 */ u16 level;  /* added to the character table's u16 at +0xE by func_00260DB8; indexes its s32 table at +0x10 in func_00260E28 */
    /* 0x36 */ u16 unk36;
} OptionCustom;

/* 0x1C bytes; fourteen of them at 0x2D40. */
typedef struct OptionRec {
    /* 0x00 */ u8 unk0[0x14];
    /* 0x14 */ u16 level;  /* used instead of OptionCustom.level when func_00260DB8 is asked for one of these */
    /* 0x16 */ u16 unk16;
    /* 0x18 */ s32 chara;  /* character id, -1 = empty (defaults; func_0025F610 appends the non-empty ones to the character list) */
} OptionRec;

typedef struct OptionBlock {
    /* 0x0000 */ s32 unk0;
    /* 0x0004 */ s32 unk4;
    /* 0x0008 */ s32 unlockFlags;   /* bits 0-6 set by Option_UnlockAll; func_0012A470 tests `1 << n` for n < 7; the menu also uses bit 9 */
    /* 0x000C */ s32 unkC;          /* 1 by default; picked from a list by menu func_0033EB88 */
    /* 0x0010 */ OptionSlot slot[OPTION_SLOT_COUNT];
    /* 0x00A0 */ u8 unkA0[0x208 - 0xA0];
    /* 0x0208 */ s32 unk208;        /* bit 0 set by Option_UnlockAll */
    /* 0x020C */ s32 unk20C;        /* 5 by default, 20 after Option_UnlockAll: a capacity, guess */
    /* 0x0210 */ u8 unk210[0x77C - 0x210]; /* menu 0x37F5F8 / 0x385268 / 0x387F90 use 0x210-0x28D */
    /* 0x077C */ s32 unk77C;        /* 99 by default */
    /* 0x0780 */ u8 unk780[0xA08 - 0x780]; /* byte records at 0x780.. (menu 0x372E98 / 0x3760C8) */
    /* 0x0A08 */ s32 unkA08;        /* flag bits (0x20, 0x40, 1 << n), menu 0x362160..0x364B18 */
    /* 0x0A0C */ s32 unkA0C;        /* counter kept in 0..23 by menu func_00362160 */
    /* 0x0A10 */ s32 unkA10;
    /* 0x0A14 */ u8 unkA14[0xC10 - 0xA14];
    /* 0x0C10 */ u64 charaBits[OPTION_CHARA_WORDS]; /* bit id: character unlocked (func_0025F610, func_00129D60) */
    /* 0x0C28 */ u64 stageBits;     /* bit id: stage unlocked (func_0025FC50 turns locked ids into 0x24) */
    /* 0x0C30 */ u32 bgmBits;       /* bit id: entry of a 25-entry list unlocked (func_0025FD60 turns locked ids into 0x19); BGM is a guess */
    /* 0x0C34 */ s32 rule[OPTION_RULE_COUNT]; /* 3, 2, 2, 0, 0, 0 by default (Option_ResetRules); read by menu 0x348710 / 0x351508 / 0x353518 */
    /* 0x0C4C */ u8 unkC4C[0x1008 - 0xC4C]; /* menu 0x359358 / 0x35BB88 index an s32 array at 0xE0C */
    /* 0x1008 */ s32 unk1008;       /* flag bits, menu func_0033ACB0 / func_00399790 */
    /* 0x100C */ s32 unk100C;       /* counter, same functions */
    /* 0x1010 */ u8 unk1010[0x1208 - 0x1010];
    /* 0x1208 */ s32 unk1208;       /* menu func_003A9DF0 */
    /* 0x120C */ u8 unk120C[0x1608 - 0x120C];
    /* 0x1608 */ s32 flags;         /* OPTION_FLAG_* */
    /* 0x160C */ s32 key[OPTION_PAD_COUNT][OPTION_KEY_COUNT];     /* button assignment used in battle: func_001D47C8 looks an action up in key[player] */
    /* 0x164C */ s32 keyEdit[OPTION_PAD_COUNT][OPTION_KEY_COUNT]; /* the copy the controller page edits and resets (menu func_003A0808 / func_003A5E48) */
    /* 0x168C */ s32 unk168C[2];
    /* 0x1694 */ s32 unk1694;       /* option copied to both players' battle setup (+0x30, +0x34) by func_00129E00 / func_00129FD8; reset with the screen page */
    /* 0x1698 */ s32 unk1698;       /* same, to +0x38 and +0x3C */
    /* 0x169C */ s32 screenX;       /* display offset, Gfx_SetDisplayRegs */
    /* 0x16A0 */ s32 screenY;
    /* 0x16A4 */ s32 soundMode;     /* 0 stereo, 1 mono: func_00261598 calls func_00124F08(soundMode ^ 1), which ends in Adx_SetMono(soundMode) */
    /* 0x16A8 */ s32 bgmVolume;     /* 0..9; sys/adx.c channels 0-1 */
    /* 0x16AC */ s32 seVolume;      /* 0..9; sys/adx.c channels 2-5 (stream SE, voice) and func_00124638 (sound effects) */
    /* 0x16B0 */ u8 unk16B0[0x1808 - 0x16B0];
    /* 0x1808 */ OptionCustom custom[OPTION_CUSTOM_COUNT];
    /* 0x2D40 */ OptionRec rec[OPTION_REC_COUNT];
    /* 0x2EC8 */ u8 item[OPTION_ITEM_COUNT]; /* OPTION_ITEM_* per item; ids are 1-based elsewhere (0x2EC7 + id) */
    /* 0x3026 */ u8 unk3026[2];
    /* 0x3028 */ s32 money;         /* 0..OPTION_MONEY_MAX */
    /* 0x302C */ u8 unk302C[OPTION_SIZE - 0x302C];
} OptionBlock;

/* Header of common file 4 (gCommonRes->data[2]): byte offsets of its tables, rounded down to 4. */
typedef struct ItemFile {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ u32 charaOffset; /* table of 0x3C-byte character entries (func_00260DB8) */
    /* 0x08 */ u32 itemOffset;  /* table of OPTION_ITEM_COUNT ItemInfo */
} ItemFile;

typedef struct ItemInfo {
    /* 0x00 */ u8 unk0[0x14];
    /* 0x14 */ s32 flags; /* ITEM_INFO_INITIAL */
    /* 0x18 */ u8 unk18[0x10];
} ItemInfo;

extern OptionBlock *gOption;

/* Word and mask of a character's unlock bit (the pointer form is what matches; `charaBits[id / 64]` does not). */
#define OPTION_CHARA_WORD(opt, id) (*((opt)->charaBits + (id) / 64))
#define OPTION_CHARA_MASK(id) (1L << ((id) % 64))

void Option_UnlockAll(OptionBlock *opt);
void Option_SetDefaults(OptionBlock *opt);
void Option_Stub266600(OptionBlock *opt);
void Option_Init(void);
void Option_AddItem(s32 idx);
void Option_AddMoney(s32 amount);
void Option_ResetRules(void);

#endif
