#include "common.h"
#include "menu/menu_i.h"

/*
 * EntrySel, 0x35E0F8..0x35F650: head of the entrant select of the tournament mode ("Dragon World Tour",
 * mode 34, run by Tour_Main 0x362160). The object goes on in the next chunk (menu_j.c); only the texture
 * helpers, the picture loader and Init are here. Its .data is the pointer gEntrySel (0x3B5910), its .rodata
 * starts at 0x3B5920.
 */

#define ES_CUR (gEntrySel->sel->entry[gEntrySel->sel->cur])
#define ES_CELL (gEntrySel->grid[ES_CUR.row * ENTRYSEL_COLS + ES_CUR.col])

/* The cursor moved to another grid row: the seven chips take the characters of the new row. */
void EntrySel_SwapRowTex(void) {
    s32 tbl[ENTRYSEL_COLS] = {8, 11, 12, 13, 14, 15, 16};
    MTexRes *res;
    s32 i;

    for (i = 0; i < ENTRYSEL_COLS; i++) {
        s32 chara =
            gEntrySel->grid[ES_CUR.row * ENTRYSEL_COLS + i].id;

        gEntrySel->sel->prevRowChara[i] = gEntrySel->sel->rowChara[i];
        gEntrySel->sel->rowChara[i] = chara;
        res = (MTexRes *)MPACK_AT(gEntrySel->chips, chara + 1);
        gEntrySel->texB[18 + i] = gEntrySel->texB[tbl[i]];
        gEntrySel->texB[tbl[i]] = MTEX(res, 0);
    }
}

/* The seven chips take the forms of the cell under the cursor (empty chips past the last form). */
void EntrySel_SetRowTex(void) {
    s32 tbl[ENTRYSEL_COLS] = {8, 11, 12, 13, 14, 15, 16};
    MTexRes *res;
    s32 i;

    for (i = 0; i < ENTRYSEL_COLS; i++) {
        s32 chara;

        if (i < ES_CELL.formCount) {
            chara = ES_CELL.form[i];
        } else {
            chara = ENTRYSEL_CHARA_NONE;
        }
        gEntrySel->sel->prevRowChara[i] = gEntrySel->sel->rowChara[i];
        gEntrySel->sel->rowChara[i] = chara;
        res = (MTexRes *)MPACK_AT(gEntrySel->chips, chara + 1);
        gEntrySel->texB[18 + i] = gEntrySel->texB[tbl[i]];
        gEntrySel->texB[tbl[i]] = MTEX(res, 0);
    }
}

/* Sets the chips of the entrants chosen so far and clears the rest. */
void EntrySel_SetMemberTex(void) {
    MTexRes *res;
    s32 i;

    for (i = 0; i < ENTRYSEL_ENTRY_MAX; i++) {
        gEntrySel->texA[10 + i] = NULL;
    }
    for (i = 0; i < gEntrySel->sel->cur; i++) {
        res = (MTexRes *)MPACK_AT(gEntrySel->chips, gEntrySel->sel->entry[i].chara + 1);
        gEntrySel->texA[10 + i] = MTEX(res, 0);
    }
}

/* One step of the background loader of the large picture (file 0x2F9 + character). */
void EntrySel_UpdateImage(void) {
    MTexRes *res;

    switch (gEntrySel->loadState) {
    case ENTRYSEL_LOAD_ABORT:
        gEntrySel->loadState = ENTRYSEL_LOAD_RESTART;
        break;
    case ENTRYSEL_LOAD_RESTART:
        if (gEntrySel->sel->flags & ENTRYSEL_IMAGE_CHANGE) {
            gEntrySel->sel->flags ^= ENTRYSEL_IMAGE_CHANGE;
        }
        gEntrySel->loadState = ENTRYSEL_LOAD_REQUEST;
        break;
    case ENTRYSEL_LOAD_REQUEST:
        File_CancelRequests();
        File_Request(gEntrySel->sel->image + 0x2F9, gEntrySel->imageFile, 0x16800);
        gEntrySel->loadState = ENTRYSEL_LOAD_READ;
        break;
    case ENTRYSEL_LOAD_READ:
        if (File_UpdateRequests()) {
            gEntrySel->loadState = ENTRYSEL_LOAD_UNPACK;
        }
        break;
    case ENTRYSEL_LOAD_UNPACK:
        Sprite_Unpack(gEntrySel->imageFile, gEntrySel->imageRes, NULL);
        res = gEntrySel->imageRes;
        Res_RelocateOffsets(&res, res, res);
        gEntrySel->texA[5] = MTEX(res, 0);
        gEntrySel->sel->flags |= ENTRYSEL_IMAGE_READY;
        gEntrySel->loadState = ENTRYSEL_LOAD_SHOWN;
        break;
    case ENTRYSEL_LOAD_SHOWN:
        if (gEntrySel->sel->flags & ENTRYSEL_IMAGE_CHANGE) {
            gEntrySel->texA[5] = NULL;
            gEntrySel->loadState = ENTRYSEL_LOAD_RESTART;
        }
        break;
    }
}

/* The shown character changed: hide the picture, or abort a load in progress, and ask for the new one. */
void EntrySel_ChangeImage(void) {
    if (gEntrySel->sel->flags & ENTRYSEL_IMAGE_READY) {
        gEntrySel->sel->flags ^= ENTRYSEL_IMAGE_READY;
    } else {
        gEntrySel->loadState = ENTRYSEL_LOAD_ABORT;
    }
    gEntrySel->sel->flags |= ENTRYSEL_IMAGE_CHANGE;
}

/* Sends a clip of movie `movie` to a label: the chip, plate or text of the entrant being chosen. */
void EntrySel_ClipGoto(s32 movie, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gEntrySel->flash[movie];

    switch (kind) {
    case 0:
        sprintf(name, "mc_chara_chip_%03d", ES_CUR.col);
        break;
    case 1:
        sprintf(name, "mc_chara_chip_%03d", ES_CUR.form);
        break;
    case 2:
        sprintf(name, "mc_custom_plate_%d", ES_CUR.custom + 1);
        break;
    case 5:
        sprintf(name, "mc_color_plate_%d", ES_CUR.costume + 1);
        break;
    case 7:
        sprintf(name, "mc_entry_text_%d", gEntrySel->sel->cur);
        break;
    case 3:
        return;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

#define ES_RES(n) \
    res = (MTexRes *)MPACK_AT(gEntrySel->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 4), builds its four movies and the grid. */
void EntrySel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 movie;
    s32 i;

    gEntrySel = Heap_Alloc(0x1C48, 0x20, 0, 2);
    memset(gEntrySel, 0, 0x1C48);
    gEntrySel->pack = (u32 *)MPACK_AT(gMenuArc4, section);
    gEntrySel->res = Sprite_Unpack(gEntrySel->pack, NULL, NULL);
    gEntrySel->file = File_LoadSync(ENTRY_PROG->tour + 0x3C9, NULL, 0);
    TourBg_Init(gEntrySel->file, ENTRY_PROG->tour, NULL);
    ES_RES(1);
    gEntrySel->texA[0] = MTEX(res, 0);
    ES_RES(2);
    gEntrySel->texA[1] = MTEX(res, 0);
    gEntrySel->texA[3] = MTEX(res, 1);
    gEntrySel->texA[18] = MTEX(res, 2);
    gEntrySel->texB[3] = MTEX(res, 1);
    ES_RES(4);
    gEntrySel->texA[4] = MTEX(res, 0);
    ES_RES(5);
    gEntrySel->texA[6] = MTEX(res, 0);
    gEntrySel->texA[7] = MTEX(res, 2);
    Flash_Create(&gEntrySel->flash[0], MPACK_AT(gEntrySel->res, 3), gEntrySel->texA);
    Flash_Play(&gEntrySel->flash[0], 1);
    ES_RES(6);
    gEntrySel->texB[0] = MTEX(res, 0);
    gEntrySel->texB[2] = MTEX(res, 1);
    ES_RES(7);
    gEntrySel->texB[1] = MTEX(res, 1);
    gEntrySel->texB[4] = MTEX(res, 2);
    gEntrySel->texB[5] = MTEX(res, 4);
    gEntrySel->texB[6] = MTEX(res, 3);
    ES_RES(8);
    gEntrySel->texB[7] = MTEX(res, 0);
    ES_RES(9);
    gEntrySel->texB[10] = MTEX(res, 0);
    gEntrySel->texB[17] = MTEX(res, 1);
    gEntrySel->texA[2] = MTEX(res, 0);
    gEntrySel->texC[4] = MTEX(res, 1);
    Flash_Create(&gEntrySel->flash[1], MPACK_AT(gEntrySel->res, 10), gEntrySel->texB);
    Flash_Play(&gEntrySel->flash[1], 1);
    ES_RES(11);
    gEntrySel->texC[9] = MTEX(res, 0);
    gEntrySel->texC[0] = MTEX(res, 1);
    ES_RES(12);
    gEntrySel->texC[7] = MTEX(res, 0);
    gEntrySel->texC[3] = MTEX(res, 1);
    ES_RES(13);
    gEntrySel->texC[2] = MTEX(res, 0);
    gEntrySel->texC[5] = MTEX(res, 1);
    ES_RES(14);
    gEntrySel->texC[6] = MTEX(res, 0);
    gEntrySel->texC[8] = MTEX(res, 1);
    ES_RES(15);
    gEntrySel->texC[10] = MTEX(res, 0);
    gEntrySel->texC[1] = MTEX(res, 1);
    Flash_Create(&gEntrySel->flash[2], MPACK_AT(gEntrySel->res, 16), gEntrySel->texC);
    Flash_Play(&gEntrySel->flash[2], 1);

    movie = 0;
    switch (ENTRY_PROG->tour) {
    case 0:
        ES_RES(17);
        gEntrySel->texD[0] = MTEX(res, 0);
        gEntrySel->texD[1] = MTEX(res, 1);
        movie = 18;
        break;
    case 1:
        ES_RES(19);
        gEntrySel->texD[0] = MTEX(res, 0);
        gEntrySel->texD[2] = MTEX(res, 1);
        gEntrySel->texD[1] = MTEX(res, 3);
        movie = 20;
        break;
    case 2:
        ES_RES(21);
        gEntrySel->texD[0] = MTEX(res, 0);
        gEntrySel->texD[2] = MTEX(res, 1);
        gEntrySel->texD[1] = MTEX(res, 3);
        movie = 22;
        break;
    case 3:
        ES_RES(23);
        gEntrySel->texD[0] = MTEX(res, 0);
        gEntrySel->texD[1] = MTEX(res, 1);
        movie = 24;
        break;
    case 4:
        ES_RES(26);
        gEntrySel->texD[0] = MTEX(res, 0);
        gEntrySel->texD[2] = MTEX(res, 1);
        gEntrySel->texD[1] = MTEX(res, 3);
        ES_RES(28);
        gEntrySel->texD[6] = MTEX(res, 0);
        gEntrySel->texD[8] = MTEX(res, 1);
        gEntrySel->texD[7] = MTEX(res, 3);
        movie = 25;
        break;
    }
    Flash_Create(&gEntrySel->flash[3], MPACK_AT(gEntrySel->res, movie), gEntrySel->texD);
    Flash_Play(&gEntrySel->flash[3], 1);
    Flash_GotoLabel(&gEntrySel->flash[3], "fl_guide_in", 1);
    ItemPanel_Init((u32 *)MPACK_AT(gEntrySel->res, 39), 0);
    func_00399240(MPACK_AT(gEntrySel->res, 35));
    gEntrySel->msgText = MPACK_AT(gEntrySel->res, 33);
    gEntrySel->subtitles = MPACK_AT(gEntrySel->res, 36);
    MsgWin_Init(MPACK_AT(gEntrySel->res, 34), gEntrySel->msgText, 0, 0);
    MsgWin_Open();
    gEntrySel->items = MPACK_AT(gCommonRes->data[2], 2);
    gEntrySel->nameText = MPACK_AT(gEntrySel->res, 30);
    gEntrySel->formText = MPACK_AT(gEntrySel->res, 31);
    gEntrySel->chips = (u32 *)MPACK_AT(gEntrySel->res, 32);
    for (i = 0; i < ENTRYSEL_CELL_MAX; i++) {
        res = (MTexRes *)MPACK_AT(gEntrySel->chips, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    gEntrySel->grid = ((MChrGridList *)MPACK_AT(gEntrySel->res, 29))->cell;
    gEntrySel->gridCount = gEntrySel->res[gEntrySel->res[29] >> 2];
    ChrGrid_Build(&gEntrySel->gridOutCount, gEntrySel->gridBuf, &gEntrySel->gridCount, gEntrySel->grid, NULL, NULL);
    gEntrySel->gridCount = gEntrySel->gridOutCount;
    gEntrySel->grid = gEntrySel->gridBuf;
    {
        s32 cols = ENTRYSEL_COLS;
        s32 cols2 = ENTRYSEL_COLS;

        gEntrySel->rows = gEntrySel->gridCount / cols;
        if (gEntrySel->gridCount % cols2) {
            gEntrySel->rows++;
        }
    }
    gEntrySel->imageFile = Heap_Alloc(0x16800, 0x40, 0, 2);
    gEntrySel->imageRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    gEntrySel->sel = &gEntrySel->state;
    gEntrySel->sel->entry[0] = ENTRY_PROG->lastEntry;
    gEntrySel->loadState = ENTRYSEL_LOAD_SHOWN;
    if (!ChrGrid_IsSelectable(gEntrySel->grid, ES_CUR.col + ES_CUR.row * ENTRYSEL_COLS)) {
        memset(gEntrySel->sel, 0, sizeof(EntrySelEntry));
        gEntrySel->sel->entry[0].col = 0;
        ENTRY_PROG->lastEntry = gEntrySel->sel->entry[0];
    }
    switch (ENTRY_PROG->tour) {
    case 0:
    case 1:
    case 2:
    case 3:
        break;
    case 4:
        memset(gEntrySel->sel, 0, sizeof(EntrySelEntry));
        for (i = 0; i < ENTRY_PROG->entryNum; i++) {
            s32 cols = ENTRYSEL_COLS;
            s32 n;

            while (1) {
                n = Rand_Range(gEntrySel->gridCount);
                if (gEntrySel->grid[n].id <= 0xA0) {
                    gEntrySel->sel->entry[i].col = n % cols;
                    gEntrySel->sel->entry[i].row = n / cols;
                    gEntrySel->sel->entry[i].custom = Rand_Range(4);
                    if (gEntrySel->grid[n].formCount != 0) {
                        gEntrySel->sel->entry[i].form = Rand_Range(gEntrySel->grid[n].formCount);
                    }
                    gEntrySel->sel->entry[i].costume = Rand_Range(ChrTbl_WrapCostume(gEntrySel->grid[n].id, NULL));
                    break;
                }
            }
        }
        break;
    }
    EntrySel_SwapRowTex();
    gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
    gEntrySel->voiceLine = -1;
    for (i = 0; i < 2; i++) {
        gEntrySel->blink[i] = Rand_Range(0x20);
    }
    File_LoadSync(gEntrySel->sel->image + 0x2F9, gEntrySel->imageFile, 0x16800);
    Sprite_Unpack(gEntrySel->imageFile, gEntrySel->imageRes, NULL);
    res = gEntrySel->imageRes;
    Res_RelocateOffsets(&res, res, res);
    gEntrySel->texA[5] = MTEX(res, 0);
    gEntrySel->sel->flags |= ENTRYSEL_IMAGE_READY;
    memset(ENTRY_PROG->entrant, 0, 0x2A8);
    TextBox_Init(&gEntrySel->box[0], gEntrySel->nameText, 1);
    TextBox_SetUnk80(&gEntrySel->box[0], 1);
    TextBox_Init(&gEntrySel->box[1], gEntrySel->formText, 3);
    TextBox_SetUnk80(&gEntrySel->box[1], 1);
}
