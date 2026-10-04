#include "common.h"
#include "menu/menu_n.h"

/*
 * UbResult, 0x3760C8..0x376920: head of the result screen of modes 27 and 30 (the object goes on in the next
 * chunk, src/menu/menu_o.c, which has the full layout; work pointer 0x3B7358). Its .rodata starts at 0x3B7E50
 * ("mc_guide_17go", first used by the next chunk); nothing here emits read-only data. Prepend this file to
 * menu_o.c.
 */

#define RS_RES(n) \
    res = (MTexRes *)MPACK_AT(gUbResult->res, n); \
    Res_RelocateOffsets(&res, res, res)

/*
 * Marks a course as cleared in the save. Returns 1 the first time all five courses are cleared (the caller then
 * gives the reward item).
 */
s32 UbResult_MarkCourseCleared(s32 course) {
    s32 count = 0;
    s32 i;

    gSaveData->course[course].cleared = 1;
    for (i = 0; i < N_COURSE_NUM; i++) {
        if (gSaveData->course[i].cleared) {
            count++;
        }
    }
    if (count == N_COURSE_NUM && !(gSaveData->unk208 & 0x10)) {
        gSaveData->unk208 |= 0x10;
        return 1;
    }
    return 0;
}

/*
 * Creates the result screen: movie, icon window, text boxes; fills the score sheet from the battle result and
 * writes what the battle earned into the save (the course's best result, the reward items).
 */
void UbResult_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    s32 rank;

    gUbResult = Heap_Alloc(sizeof(NResult), 0x20, 0, 2);
    memset(gUbResult, 0, sizeof(NResult));
    gUbResult->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gUbResult->res = Sprite_Unpack(gUbResult->pack, NULL, NULL);
    RS_RES(5);
    gUbResult->bg = res;
    RS_RES(7);
    gUbResult->tex[39] = MTEX(res, 0);
    gUbResult->tex[40] = MTEX(res, 1);
    gUbResult->tex[41] = MTEX(res, 3);
    RS_RES(4);
    gUbResult->tex[0] = MTEX(res, 0);
    gUbResult->tex[1] = MTEX(res, 3);
    gUbResult->tex[30] = MTEX(res, 6);
    gUbResult->tex[21] = MTEX(res, 7);
    gUbResult->tex[18] = MTEX(res, 8);
    gUbResult->tex[10] = MTEX(res, 9);
    gUbResult->tex[9] = MTEX(res, 10);
    gUbResult->tex[38] = MTEX(res, 12);
    gUbResult->tex[35] = MTEX(res, 13);
    gUbResult->tex[36] = MTEX(res, 14);
    gUbResult->tex[33] = MTEX(res, 15);
    gUbResult->tex[32] = MTEX(res, 16);
    gUbResult->tex[34] = MTEX(res, 17);
    gUbResult->tex[23] = MTEX(res, 18);
    gUbResult->tex[17] = MTEX(res, 19);
    gUbResult->tex[26] = MTEX(res, 20);
    gUbResult->tex[27] = MTEX(res, 21);
    gUbResult->tex[20] = MTEX(res, 22);
    gUbResult->tex[29] = MTEX(res, 23);
    gUbResult->tex[16] = MTEX(res, 25);
    gUbResult->tex[24] = MTEX(res, 26);
    gUbResult->tex[3] = MTEX(res, 27);
    gUbResult->tex[2] = MTEX(res, 28);
    gUbResult->tex[19] = MTEX(res, 29);
    gUbResult->tex[28] = MTEX(res, 30);
    gUbResult->tex[4] = MTEX(res, 31);
    gUbResult->tex[5] = MTEX(res, 32);
    gUbResult->tex[11] = MTEX(res, 33);
    gUbResult->tex[15] = MTEX(res, 34);
    gUbResult->tex[14] = MTEX(res, 35);
    gUbResult->tex[6] = MTEX(res, 36);
    gUbResult->tex[8] = MTEX(res, 37);
    gUbResult->tex[7] = MTEX(res, 38);
    gUbResult->tex[25] = MTEX(res, 39);
    gUbResult->tex[31] = MTEX(res, 43);
    gUbResult->tex[45] = MTEX(res, 44);
    gUbResult->tex[42] = MTEX(res, 45);
    gUbResult->tex[43] = MTEX(res, 46);
    gUbResult->tex[44] = MTEX(res, 47);
    gUbResult->tex[12] = NULL;
    gUbResult->tex[13] = NULL;
    gUbResult->tex[22] = NULL;
    gUbResult->tex[37] = NULL;
    gUbResult->tex[46] = NULL;
    Flash_Create(&gUbResult->flash[0], MPACK_AT(gUbResult->res, 1), gUbResult->tex);
    Flash_Play(&gUbResult->flash[0], 1);
    gUbResult->blink = Rand_Libc() % 32;
    RS_RES(8);
    IconWin_Init(MPACK_AT(gUbResult->res, 9), res);
    IconWin_Open();
    gUbResult->text = MPACK_AT(gUbResult->res, 12);
    gUbResult->subtitles = MPACK_AT(gUbResult->res, 11);
    gUbResult->itemText = MPACK_AT(gUbResult->res, 14);
    for (i = 0; i < 4; i++) {
        TextBox_Init(&gUbResult->box[i], gUbResult->text, 0);
        TextBox_SetUnk50(&gUbResult->box[i], 0);
    }
    TextBox_Init(&gUbResult->itemBox, gUbResult->itemText, 0);
    TextBox_SetUnk50(&gUbResult->itemBox, 0);
    TextBox_SetLineOffsets(&gUbResult->itemBox, 12, 0, 0, 0, 0);
    gUbResult->bonusTbl = MPACK_AT(gUbResult->res, 13);
    gUbResult->voiceLine = -1;
    gUbResult->page = 0;
    gUbResult->timer = 15;
    gUbResult->pageDone = 0;
    gUbResult->counting = 0;
    gUbResult->count = 0;
    gUbResult->gotItem = 0;
    gUbResult->skip = 0;
    gUbResult->kind = NPROG->ubKind;
    gUbResult->lose = UbScore_Fill(3, gUbResult->score, &gUbResult->pageCount);
    UbScore_CalcPoints(gUbResult->bonusTbl, gUbResult->score);
    rank = UbScore_CalcRank(3, gUbResult->score);
    if (gUbResult->kind == 1) {
        /* a course */
        gUbResult->course = NPROG->ubChoice;
        IconWin_SetIcon(4);
        if (gUbResult->lose == 0) {
            gUbResult->newRecord = UbScore_SaveBestC(gUbResult->course, rank, gUbResult->score);
            if (UbResult_MarkCourseCleared(gUbResult->course)) {
                Save_AddItem(UbScore_GetRewardItem(5));
                gUbResult->gotItem = 1;
            }
        }
    } else {
        /* the ladder: first place taken */
        IconWin_SetIcon(3);
        if (gUbResult->lose == 0 && gSaveData->rank == 1 && (NPROG->ubFlags & NPROG_UB_UPWARD)) {
            gUbResult->gotItem = 1;
            Save_AddItem(UbScore_GetRewardItem(4));
        }
    }
}
