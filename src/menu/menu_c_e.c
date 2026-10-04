#include "common.h"
#include "menu/menu_a.h"

/*
 * 0x342190..0x342588: the first three functions of the character / stage select object (CharSel), whose body
 * is src/menu/menu_d.c. Its read-only data starts with these functions' tables at 0x3B38F0, and its .data
 * (gCharSel and six more words) is at 0x3B38D4, right behind the previous object's jump table.
 *
 * Local view of the work area: include/menu/menu_d.h has the full layout (same offsets); it belongs to
 * another chunk and was still changing, so only what these functions touch is declared here.
 */

typedef struct CsCell {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 formCount;
    /* 0x08 */ s32 form[7];
} CsCell; /* 0x24 */

typedef struct CsSide {
    /* 0x00 */ s32 col;
    /* 0x04 */ s32 row;
    /* 0x08 */ s32 unk8[10];
    /* 0x30 */ s32 chip[7];
    /* 0x4C */ s32 prevChip[7];
} CsSide;

typedef struct CsStage {
    /* 0x00 */ s32 col;
    /* 0x04 */ s32 row;
    /* 0x08 */ s32 chip[2][6];   /* stage ids on the six chips of the reel: [0] now, [1] before the last change */
} CsStage;

typedef struct CsWork {
    /* 0x0000 */ u8 unk0[0x24];
    /* 0x0024 */ u32 *chipPack;
    /* 0x0028 */ u8 unk28[8];
    /* 0x0030 */ u32 *stagePack;
    /* 0x0034 */ u8 unk34[0x118 - 0x34];
    /* 0x0118 */ u8 *tex[49];
    /* 0x01DC */ u8 *plateTex[2][11];
    /* 0x0234 */ u8 *sideTex[2][25];
    /* 0x02FC */ u8 unk2FC[0x4F8 - 0x2FC];
    /* 0x04F8 */ CsSide *side[2];
    /* 0x0500 */ u8 unk500[0x54];
    /* 0x0554 */ CsStage *stage;
    /* 0x0558 */ s32 unk558[2];
    /* 0x0560 */ CsCell *cells[2];
    /* 0x0568 */ u8 unk568[0x3804 - 0x568];
    /* 0x3804 */ s32 *stageIds;
} CsWork;

#define CS_EMPTY 0xA4

extern CsWork *gCharSel;

/* The six chips of the stage reel show the cursor's row of the stage grid. */
void CharSel_SetStageChips(void) {
    s32 slot[6] = { 8, 11, 12, 13, 14, 15 };
    s32 i;

    for (i = 0; i < 6; i++) {
        s32 id = gCharSel->stageIds[i + gCharSel->stage->row * 6];
        MTexRes *res;

        gCharSel->stage->chip[1][i] = gCharSel->stage->chip[0][i];
        gCharSel->stage->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gCharSel->stagePack, id + 1);
        gCharSel->tex[41 + i] = gCharSel->tex[slot[i]];
        gCharSel->tex[slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the cursor's row of the character grid. */
void CharSel_SetRowChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id = gCharSel->cells[side][i + gCharSel->side[side]->row * 7].id;
        MTexRes *res;

        gCharSel->side[side]->prevChip[i] = gCharSel->side[side]->chip[i];
        gCharSel->side[side]->chip[i] = id;
        res = (MTexRes *)MPACK_AT(gCharSel->chipPack, id + 1);
        gCharSel->sideTex[side][18 + i] = gCharSel->sideTex[side][slot[i]];
        gCharSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the forms of the grid cell under the cursor. */
void CharSel_SetCellFormChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id;
        MTexRes *res;

        if (i < gCharSel->cells[side][gCharSel->side[side]->col + gCharSel->side[side]->row * 7].formCount) {
            id = gCharSel->cells[side][gCharSel->side[side]->col + gCharSel->side[side]->row * 7].form[i];
        } else {
            id = CS_EMPTY;
        }
        gCharSel->side[side]->prevChip[i] = gCharSel->side[side]->chip[i];
        gCharSel->side[side]->chip[i] = id;
        res = (MTexRes *)MPACK_AT(gCharSel->chipPack, id + 1);
        gCharSel->sideTex[side][18 + i] = gCharSel->sideTex[side][slot[i]];
        gCharSel->sideTex[side][slot[i]] = res->tex;
    }
}
