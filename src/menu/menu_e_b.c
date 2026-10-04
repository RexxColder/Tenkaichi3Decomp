#include "common.h"
#include "menu/menu_e.h"

/*
 * TeamSel: the team character / stage / music select of the versus modes, 0x348D78..0x34D368 (head of the
 * screen; TeamSel_Update, TeamSel_Input and TeamSel_Run follow in the next chunk). Two sides choose up to five
 * fighters each from the character grid (form reel, custom-character list, item-set plates, costume plates),
 * optionally under a DP limit (battleType 2: each character has a cost and the team total may not exceed
 * 10 / 15 / 20), then the stage and the music. It is the team version of CharSel (0x342190..0x348D78) and
 * shares its movies, grids and loaders.
 */

extern s32 ChrTbl_GetCost(s32 chara);
extern s32 ChrTbl_IsRelated(s32 a, s32 b);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern s32 ChrGrid_IsSelectable(TsCell *cells, s32 index);
extern s32 StgGrid_IsSelectable(s32 *ids, s32 index);
extern void ChrGrid_Build(s32 *outCount, TsCell *out, s32 *inCount, TsCell *in, s32 *customCount, TsCell *custom);
extern void StgGrid_ApplyUnlocks(s32 *count, s32 *ids);
extern void BgmList_ApplyUnlocks(s32 *count, s32 *ids);

/* First word of a section of a pack (the count of a list section, whose entries start at +0x10). */
#define TS_PACK_WORD(pack, n) (*(s32 *)(((((u32 *)(pack))[n] >> 2) << 2) + (u32)(pack)))

/* The common resources of the main executable (include/sys/common.h). */
typedef struct TsCommonRes {
    /* 0x00 */ void *boot;
    /* 0x04 */ u32 *data[3]; /* files 2, 3, 4 */
} TsCommonRes;
extern TsCommonRes *gCommonRes;

/* Neighbours in the overlay: the item panel of a side (0x351C38.., names from config/symbols/menu_g.txt) and the
   item help window (0x399240.., not named yet). */
extern void ItemPanel_Init(void *data, s32 side);
extern void ItemPanel_Term(s32 side);
extern void ItemPanel_Draw(s32 side);
extern void func_00399240(void *data);
extern void func_00399430(void);
extern void func_00399478(s32 arg);

/* DP battle: adds up the cost of a side's members; with skipCur the member being chosen is left out. */
void TeamSel_SumCost(s32 side, s32 skipCur) {
    s32 i;

    if (gTeamSel->battleType == 2) {
        gTeamSel->side[side]->cost = 0;
        for (i = 0; i < gTeamSel->side[side]->memberCount; i++) {
            if (i != gTeamSel->side[side]->cur || !skipCur) {
                gTeamSel->side[side]->cost += ChrTbl_GetCost(gTeamSel->side[side]->member[i].chara);
            }
        }
    }
}

/* Returns 0 if another member of the side's team is the same person as `chara` (a form of the same character). */
s32 TeamSel_IsCharaFree(s32 side, s32 chara) {
    s32 ok = 1;
    s32 i;

    for (i = 0; i < gTeamSel->side[side]->memberCount; i++) {
        if (i != gTeamSel->side[side]->cur) {
            if (ChrTbl_IsRelated(chara, gTeamSel->side[side]->member[i].chara)) {
                ok = 0;
            }
        }
    }
    return ok;
}

/* DP battle: whether the side's team can still afford `chara`. Always 1 otherwise and for the special cells. */
s32 TeamSel_FitsDp(s32 side, s32 chara) {
    TeamSel *w = gTeamSel;

    if (w->battleType != 2) {
        return 1;
    }
    if (chara == TS_RANDOM || chara == TS_CUSTOM) {
        return 1;
    }
    return gTeamSel->side[side]->cost + ChrTbl_GetCost(chara) <= w->dpMax;
}

/* Takes member `idx` out of a side's team: the members behind it move up and the last slot is emptied. */
void TeamSel_RemoveMember(s32 side, s32 idx) {
    s32 i;

    memset(&gTeamSel->side[side]->member[idx], 0, sizeof(TsMember));
    gTeamSel->side[side]->member[idx].chara = -1;
    if (idx < gTeamSel->side[side]->memberCount) {
        for (i = idx; i < gTeamSel->side[side]->memberCount - 1; i++) {
            gTeamSel->side[side]->member[i] = gTeamSel->side[side]->member[i + 1];
        }
        memset(&gTeamSel->side[side]->member[gTeamSel->side[side]->memberCount - 1], 0, sizeof(TsMember));
        gTeamSel->side[side]->member[gTeamSel->side[side]->memberCount - 1].chara = -1;
    }
}

/* The six stage chips show the cursor's row of the stage grid. */
void TeamSel_SetStageChips(void) {
    s32 slot[6] = { 8, 11, 12, 13, 14, 15 };
    s32 i;

    for (i = 0; i < 6; i++) {
        s32 id = gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + i];
        MTexRes *res;

        gTeamSel->stage->chip[1][i] = gTeamSel->stage->chip[0][i];
        gTeamSel->stage->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gTeamSel->stagePack, id + 1);
        gTeamSel->tex[41 + i] = gTeamSel->tex[slot[i]];
        gTeamSel->tex[slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the cursor's row of the character grid. */
void TeamSel_SetChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id = gTeamSel->cells[side][gTeamSel->side[side]->member[gTeamSel->side[side]->cur].row * 7 + i].id;
        MTexRes *res;

        gTeamSel->side[side]->chip[1][i] = gTeamSel->side[side]->chip[0][i];
        gTeamSel->side[side]->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, id + 1);
        gTeamSel->sideTex[side][18 + i] = gTeamSel->sideTex[side][slot[i]];
        gTeamSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the forms of the grid cell under the cursor. */
void TeamSel_SetGridFormChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id;
        MTexRes *res;

        if (i < gTeamSel->cells[side][gTeamSel->side[side]->member[gTeamSel->side[side]->cur].col +
                                      gTeamSel->side[side]->member[gTeamSel->side[side]->cur].row * 7].formCount) {
            id = gTeamSel->cells[side][gTeamSel->side[side]->member[gTeamSel->side[side]->cur].col +
                                       gTeamSel->side[side]->member[gTeamSel->side[side]->cur].row * 7].form[i];
        } else {
            id = TS_EMPTY;
        }
        gTeamSel->side[side]->chip[1][i] = gTeamSel->side[side]->chip[0][i];
        gTeamSel->side[side]->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, id + 1);
        gTeamSel->sideTex[side][18 + i] = gTeamSel->sideTex[side][slot[i]];
        gTeamSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show a row of the custom-character list. */
void TeamSel_SetCustomChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id = gTeamSel->custom[side][i + gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customRow * 7].id;
        MTexRes *res;

        gTeamSel->side[side]->chip[1][i] = gTeamSel->side[side]->chip[0][i];
        gTeamSel->side[side]->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, id + 1);
        gTeamSel->sideTex[side][18 + i] = gTeamSel->sideTex[side][slot[i]];
        gTeamSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* The seven chips of a side's reel show the forms of the custom cell under the cursor. */
void TeamSel_SetFormChips(s32 side) {
    s32 slot[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id;
        MTexRes *res;

        if (i < gTeamSel->custom[side][gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customCol +
                                       gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customRow * 7].formCount) {
            id = gTeamSel->custom[side][gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customCol +
                                        gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customRow * 7].form[i];
        } else {
            id = TS_EMPTY;
        }
        gTeamSel->side[side]->chip[1][i] = gTeamSel->side[side]->chip[0][i];
        gTeamSel->side[side]->chip[0][i] = id;
        res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, id + 1);
        gTeamSel->sideTex[side][18 + i] = gTeamSel->sideTex[side][slot[i]];
        gTeamSel->sideTex[side][slot[i]] = res->tex;
    }
}

/* Puts the chips of a side's members on its team movie and, in a DP battle, adds up their cost. */
void TeamSel_SetTeamTex(s32 side) {
    s32 slot[5] = { 1, 4, 5, 6, 7 };
    s32 i;

    if (gTeamSel->battleType == 2) {
        gTeamSel->side[side]->cost = 0;
    }
    for (i = 0; i < 5; i++) {
        gTeamSel->teamTex[side][slot[i]] = NULL;
        if (i < gTeamSel->side[side]->memberCount) {
            s32 id = gTeamSel->side[side]->member[i].chara;
            MTexRes *res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, id + 1);

            gTeamSel->teamTex[side][slot[i]] = res->tex;
            if (gTeamSel->battleType == 2) {
                gTeamSel->side[side]->cost += ChrTbl_GetCost(id);
            }
        }
    }
}

/* One step of the background loader of the two portraits. */
void TeamSel_UpdateFaceLoad(void) {
    MTexRes *res;

    switch (gTeamSel->faceState) {
    case TEAMSEL_LOAD_ABORT:
        if (gTeamSel->side[gTeamSel->faceSide ^ 1]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
            gTeamSel->faceSide ^= 1;
        }
        gTeamSel->faceState = TEAMSEL_LOAD_RESTART;
        break;
    case TEAMSEL_LOAD_RESTART:
        if (gTeamSel->side[gTeamSel->faceSide]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
            gTeamSel->side[gTeamSel->faceSide]->flags ^= TEAMSEL_SIDE_FACE_CHANGE;
        }
        gTeamSel->faceState = TEAMSEL_LOAD_REQUEST;
        break;
    case TEAMSEL_LOAD_REQUEST:
        File_CancelRequests();
        if (gTeamSel->side[gTeamSel->faceSide]->chara < 0) {
            gTeamSel->side[gTeamSel->faceSide]->flags |= TEAMSEL_SIDE_NO_FACE;
            gTeamSel->side[gTeamSel->faceSide]->flags |= TEAMSEL_SIDE_FACE_READY;
            if (gTeamSel->side[gTeamSel->faceSide ^ 1]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
                gTeamSel->faceSide ^= 1;
                gTeamSel->faceState = TEAMSEL_LOAD_RESTART;
            } else {
                gTeamSel->faceState = TEAMSEL_LOAD_IDLE;
            }
        } else {
            File_Request(gTeamSel->side[gTeamSel->faceSide]->chara + TS_FACE_FILE,
                         gTeamSel->faceFile[gTeamSel->faceSide], 0x16800);
            gTeamSel->faceState = TEAMSEL_LOAD_READ;
        }
        break;
    case TEAMSEL_LOAD_READ:
        if (File_UpdateRequests()) {
            gTeamSel->faceState = TEAMSEL_LOAD_UNPACK;
        }
        break;
    case TEAMSEL_LOAD_UNPACK:
        Sprite_Unpack(gTeamSel->faceFile[gTeamSel->faceSide], gTeamSel->faceRes[gTeamSel->faceSide], NULL);
        res = gTeamSel->faceRes[gTeamSel->faceSide];
        Res_RelocateOffsets(&res, res, res);
        if (gTeamSel->faceSide == 0) {
            gTeamSel->tex[23] = MTEX(res, 0);
        } else {
            gTeamSel->tex[22] = MTEX(res, 0);
        }
        gTeamSel->side[gTeamSel->faceSide]->flags |= TEAMSEL_SIDE_FACE_READY;
        if (gTeamSel->side[gTeamSel->faceSide ^ 1]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
            gTeamSel->faceSide ^= 1;
            gTeamSel->faceState = TEAMSEL_LOAD_RESTART;
        } else {
            gTeamSel->faceState = TEAMSEL_LOAD_IDLE;
        }
        break;
    case TEAMSEL_LOAD_IDLE:
        if (gTeamSel->stageState != TEAMSEL_LOAD_IDLE) {
            break;
        }
        if (gTeamSel->side[0]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
            gTeamSel->tex[23] = NULL;
            gTeamSel->faceSide = 0;
            gTeamSel->faceState = TEAMSEL_LOAD_RESTART;
        } else if (gTeamSel->side[1]->flags & TEAMSEL_SIDE_FACE_CHANGE) {
            gTeamSel->tex[22] = NULL;
            gTeamSel->faceSide = 1;
            gTeamSel->faceState = TEAMSEL_LOAD_RESTART;
        }
        break;
    }
}

/* A side's character changed: hide its portrait, or abort its load in progress, and ask for the new one. */
void TeamSel_RequestFace(s32 side) {
    if (gTeamSel->side[side]->flags & TEAMSEL_SIDE_FACE_READY) {
        gTeamSel->side[side]->flags ^= TEAMSEL_SIDE_FACE_READY;
    } else if (gTeamSel->faceSide == side) {
        gTeamSel->faceState = TEAMSEL_LOAD_ABORT;
    }
    if (gTeamSel->side[side]->flags & TEAMSEL_SIDE_NO_FACE) {
        gTeamSel->side[side]->flags ^= TEAMSEL_SIDE_NO_FACE;
    }
    gTeamSel->side[side]->flags |= TEAMSEL_SIDE_FACE_CHANGE;
}

/* One step of the background loader of the stage picture. */
void TeamSel_UpdateStageLoad(void) {
    MTexRes *res;

    switch (gTeamSel->stageState) {
    case TEAMSEL_LOAD_ABORT:
        gTeamSel->stageState = TEAMSEL_LOAD_RESTART;
        break;
    case TEAMSEL_LOAD_RESTART:
        if (gTeamSel->flags & TEAMSEL_STAGE_CHANGE) {
            gTeamSel->flags ^= TEAMSEL_STAGE_CHANGE;
        }
        gTeamSel->stageState = TEAMSEL_LOAD_REQUEST;
        break;
    case TEAMSEL_LOAD_REQUEST:
        File_CancelRequests();
        File_Request(gTeamSel->stage->stage + TS_STAGE_FILE, gTeamSel->stageFile, 0x3B800);
        gTeamSel->stageState = TEAMSEL_LOAD_READ;
        break;
    case TEAMSEL_LOAD_READ:
        if (File_UpdateRequests()) {
            gTeamSel->stageState = TEAMSEL_LOAD_UNPACK;
        }
        break;
    case TEAMSEL_LOAD_UNPACK:
        Sprite_Unpack(gTeamSel->stageFile, gTeamSel->stageRes[gTeamSel->stageBuf], NULL);
        res = gTeamSel->stageRes[gTeamSel->stageBuf];
        Res_RelocateOffsets(&res, res, res);
        gTeamSel->bg[0] = res;
        gTeamSel->stageBuf ^= 1;
        gTeamSel->flags |= TEAMSEL_STAGE_READY;
        gTeamSel->stageState = TEAMSEL_LOAD_IDLE;
        break;
    case TEAMSEL_LOAD_IDLE:
        if (gTeamSel->faceState != TEAMSEL_LOAD_IDLE) {
            break;
        }
        if (gTeamSel->flags & TEAMSEL_STAGE_CHANGE) {
            gTeamSel->stageState = TEAMSEL_LOAD_RESTART;
        }
        break;
    }
}

/* The stage under the cursor changed: start the cross fade, or abort a load in progress, and ask for the new picture. */
void TeamSel_RequestStage(void) {
    if (gTeamSel->flags & TEAMSEL_STAGE_READY) {
        gTeamSel->flags ^= TEAMSEL_STAGE_READY;
    } else {
        gTeamSel->stageState = TEAMSEL_LOAD_ABORT;
    }
    gTeamSel->flags |= TEAMSEL_STAGE_CHANGE;
    gTeamSel->bg[0] = gTeamSel->stageRes[gTeamSel->stageBuf];
    gTeamSel->bg[1] = gTeamSel->stageRes[gTeamSel->stageBuf ^ 1];
}

/* Sends the clip of one chip or plate of a movie to a label; `kind` says which clip and from which cursor. */
void TeamSel_ClipGoto(s32 flash, s32 side, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *f = &gTeamSel->flash[flash];

    switch (kind) {
    case TEAMSEL_CLIP_CHIP:
        sprintf(name, "mc_chara_chip_%03d", gTeamSel->side[side]->member[gTeamSel->side[side]->cur].col);
        break;
    case TEAMSEL_CLIP_FORM_CHIP:
        sprintf(name, "mc_chara_chip_%03d", gTeamSel->side[side]->member[gTeamSel->side[side]->cur].form);
        break;
    case TEAMSEL_CLIP_CUSTOM_PLATE:
        sprintf(name, "mc_custom_plate_%d", gTeamSel->side[side]->member[gTeamSel->side[side]->cur].plate + 1);
        break;
    case TEAMSEL_CLIP_COLOR_PLATE:
        sprintf(name, "mc_color_plate_%d", gTeamSel->side[side]->member[gTeamSel->side[side]->cur].color + 1);
        break;
    case TEAMSEL_CLIP_TEAM:
        switch (gTeamSel->side[side]->cur) {
        case TS_MEMBER_MAX:
            sprintf(name, "mc_menu_plate");
            break;
        default:
            sprintf(name, "mc_team_%d", gTeamSel->side[side]->cur);
            break;
        }
        break;
    case TEAMSEL_CLIP_CUSTOM_CHIP:
        sprintf(name, "mc_chara_chip_%03d", gTeamSel->side[side]->member[gTeamSel->side[side]->cur].customCol);
        break;
    case TEAMSEL_CLIP_STAGE_CHIP:
        if (gTeamSel->stage->col == 6) {
            sprintf(name, "mc_bgm_now");
        } else {
            sprintf(name, "mc_map_chip_%02d", gTeamSel->stage->col);
        }
        break;
    case 3:
    case 10:
        return;
    case 4:
    case 8:
    default:
        /* original oddity: these kinds go on with `name` never written */
        break;
    }
    Flash_FindLabel(f, NULL, name, &ref);
    Flash_ClipGotoLabel(f, &ref, label);
}

#define TS_RES(n) \
    res = (MTexRes *)MPACK_AT(gTeamSel->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 5), builds the grids and restores the last choices. */
void TeamSel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    s32 j;

    gTeamSel = Heap_Alloc(sizeof(TeamSel), 0x20, 0, 2);
    memset(gTeamSel, 0, sizeof(TeamSel));
    gTeamSel->pack = (u32 *)MPACK_AT(gMenuArc5, section);
    gTeamSel->res = Sprite_Unpack(gTeamSel->pack, NULL, NULL);
    TS_RES(47);
    gTeamSel->tex[0] = MTEX(res, 0);
    TS_RES(1);
    gTeamSel->tex[17] = MTEX(res, 0);
    gTeamSel->tex[16] = MTEX(res, 1);
    TS_RES(2);
    gTeamSel->tex[18] = MTEX(res, 1);
    TS_RES(3);
    gTeamSel->tex[20] = MTEX(res, 0);
    TS_RES(4);
    gTeamSel->tex[19] = MTEX(res, 0);
    TS_RES(5);
    gTeamSel->tex[9] = MTEX(res, 0);
    gTeamSel->tex[40] = MTEX(res, 1);
    if (gTsProgress->players == 1) {
        TS_RES(50);
        gTeamSel->tex[10] = MTEX(res, 0);
    }
    TS_RES(6);
    gTeamSel->tex[39] = MTEX(res, 0);
    TS_RES(7);
    gTeamSel->tex[38] = MTEX(res, 0);
    TS_RES(8);
    gTeamSel->tex[2] = MTEX(res, 1);
    gTeamSel->tex[4] = MTEX(res, 2);
    gTeamSel->tex[5] = MTEX(res, 4);
    gTeamSel->tex[6] = MTEX(res, 3);
    TS_RES(9);
    gTeamSel->tex[3] = MTEX(res, 1);
    gTeamSel->tex[1] = MTEX(res, 0);
    TS_RES(10);
    gTeamSel->tex[21] = MTEX(res, 0);
    TS_RES(11);
    gTeamSel->tex[24] = MTEX(res, 0);
    gTeamSel->tex[26] = MTEX(res, 1);
    gTeamSel->tex[25] = MTEX(res, 2);
    gTeamSel->tex[27] = MTEX(res, 3);
    TS_RES(12);
    gTeamSel->tex[47] = MTEX(res, 0);
    gTeamSel->tex[48] = MTEX(res, 1);
    if (gTsProgress->battleType == 2) {
        TS_RES(13);
        gTeamSel->tex[28] = MTEX(res, 0);
        gTeamSel->tex[31] = MTEX(res, 1);
        TS_RES(14);
        gTeamSel->tex[29] = MTEX(res, 0);
        gTeamSel->tex[30] = MTEX(res, 1);
    }
    Flash_Create(&gTeamSel->flash[0], MPACK_AT(gTeamSel->res, 15), gTeamSel->tex);
    Flash_Play(&gTeamSel->flash[0], 1);
    if (gTsProgress->battleType == 2) {
        TS_RES(16);
        gTeamSel->sideTex[0][9] = MTEX(res, 2);
        gTeamSel->sideTex[1][9] = MTEX(res, 2);
        gTeamSel->teamTex[0][2] = MTEX(res, 2);
        gTeamSel->teamTex[1][2] = MTEX(res, 2);
    }
    TS_RES(17);
    gTeamSel->sideTex[0][0] = MTEX(res, 0);
    gTeamSel->sideTex[0][2] = MTEX(res, 1);
    TS_RES(18);
    gTeamSel->sideTex[1][0] = MTEX(res, 0);
    gTeamSel->sideTex[1][2] = MTEX(res, 1);
    TS_RES(19);
    gTeamSel->sideTex[0][1] = MTEX(res, 1);
    gTeamSel->sideTex[0][4] = MTEX(res, 2);
    gTeamSel->sideTex[0][5] = MTEX(res, 4);
    gTeamSel->sideTex[0][6] = MTEX(res, 3);
    TS_RES(20);
    gTeamSel->sideTex[1][1] = MTEX(res, 1);
    gTeamSel->sideTex[1][4] = MTEX(res, 2);
    gTeamSel->sideTex[1][5] = MTEX(res, 4);
    gTeamSel->sideTex[1][6] = MTEX(res, 3);
    TS_RES(21);
    gTeamSel->sideTex[0][7] = MTEX(res, 0);
    gTeamSel->sideTex[1][7] = MTEX(res, 0);
    gTeamSel->tex[7] = MTEX(res, 0);
    TS_RES(22);
    gTeamSel->sideTex[0][3] = MTEX(res, 0);
    gTeamSel->sideTex[1][3] = MTEX(res, 0);
    TS_RES(23);
    gTeamSel->sideTex[0][10] = MTEX(res, 0);
    gTeamSel->sideTex[0][17] = MTEX(res, 1);
    gTeamSel->sideTex[1][10] = MTEX(res, 0);
    gTeamSel->sideTex[1][17] = MTEX(res, 1);
    gTeamSel->plateTex[0][4] = MTEX(res, 1);
    gTeamSel->plateTex[1][4] = MTEX(res, 1);
    gTeamSel->teamTex[0][3] = MTEX(res, 0);
    gTeamSel->teamTex[1][3] = MTEX(res, 0);
    Flash_Create(&gTeamSel->flash[3], MPACK_AT(gTeamSel->res, 24), gTeamSel->sideTex[0]);
    Flash_Play(&gTeamSel->flash[3], 1);
    Flash_Create(&gTeamSel->flash[4], MPACK_AT(gTeamSel->res, 25), gTeamSel->sideTex[1]);
    Flash_Play(&gTeamSel->flash[4], 1);
    TS_RES(48);
    gTeamSel->plateTex[0][9] = MTEX(res, 0);
    gTeamSel->plateTex[0][0] = MTEX(res, 1);
    TS_RES(49);
    gTeamSel->plateTex[1][9] = MTEX(res, 0);
    gTeamSel->plateTex[1][0] = MTEX(res, 1);
    TS_RES(26);
    gTeamSel->plateTex[0][7] = MTEX(res, 0);
    gTeamSel->plateTex[0][3] = MTEX(res, 1);
    TS_RES(27);
    gTeamSel->plateTex[1][7] = MTEX(res, 0);
    gTeamSel->plateTex[1][3] = MTEX(res, 1);
    TS_RES(28);
    gTeamSel->plateTex[0][2] = MTEX(res, 0);
    gTeamSel->plateTex[0][5] = MTEX(res, 1);
    gTeamSel->plateTex[1][2] = MTEX(res, 0);
    gTeamSel->plateTex[1][5] = MTEX(res, 1);
    TS_RES(29);
    gTeamSel->plateTex[0][6] = MTEX(res, 0);
    gTeamSel->plateTex[0][8] = MTEX(res, 1);
    gTeamSel->plateTex[1][6] = MTEX(res, 0);
    gTeamSel->plateTex[1][8] = MTEX(res, 1);
    TS_RES(30);
    gTeamSel->plateTex[0][10] = MTEX(res, 0);
    gTeamSel->plateTex[0][1] = MTEX(res, 1);
    TS_RES(31);
    gTeamSel->plateTex[1][10] = MTEX(res, 0);
    gTeamSel->plateTex[1][1] = MTEX(res, 1);
    Flash_Create(&gTeamSel->flash[5], MPACK_AT(gTeamSel->res, 32), gTeamSel->plateTex[0]);
    Flash_Play(&gTeamSel->flash[5], 1);
    Flash_Create(&gTeamSel->flash[6], MPACK_AT(gTeamSel->res, 33), gTeamSel->plateTex[1]);
    Flash_Play(&gTeamSel->flash[6], 1);
    TS_RES(34);
    gTeamSel->teamTex[0][0] = MTEX(res, 0);
    gTeamSel->teamTex[1][0] = MTEX(res, 0);
    TS_RES(35);
    gTeamSel->teamTex[0][8] = MTEX(res, 0);
    gTeamSel->teamTex[0][11] = MTEX(res, 1);
    gTeamSel->teamTex[0][10] = MTEX(res, 2);
    gTeamSel->teamTex[1][8] = MTEX(res, 0);
    gTeamSel->teamTex[1][11] = MTEX(res, 1);
    gTeamSel->teamTex[1][10] = MTEX(res, 2);
    TS_RES(36);
    gTeamSel->teamTex[0][9] = MTEX(res, 0);
    gTeamSel->teamTex[0][12] = MTEX(res, 1);
    gTeamSel->teamTex[1][9] = MTEX(res, 0);
    gTeamSel->teamTex[1][12] = MTEX(res, 1);
    Flash_Create(&gTeamSel->flash[1], MPACK_AT(gTeamSel->res, 37), gTeamSel->teamTex[0]);
    Flash_Play(&gTeamSel->flash[1], 1);
    Flash_Create(&gTeamSel->flash[2], MPACK_AT(gTeamSel->res, 38), gTeamSel->teamTex[1]);
    Flash_Play(&gTeamSel->flash[2], 1);
    {
        void *panel = MPACK_AT(gTeamSel->res, 53);

        ItemPanel_Init(panel, 0);
        ItemPanel_Init(panel, 1);
    }
    func_00399240(MPACK_AT(gTeamSel->res, 51));
    TS_RES(55);
    IconWin_Init(MPACK_AT(gTeamSel->res, 54), res);
    gTeamSel->unk3EA0 = MPACK_AT(gCommonRes->data[2], 2);
    gTeamSel->nameText = MPACK_AT(gTeamSel->res, 43);
    gTeamSel->formText = MPACK_AT(gTeamSel->res, 44);
    gTeamSel->chipPack = (u32 *)MPACK_AT(gTeamSel->res, 45);
    for (i = 0; i < TS_CELL_MAX; i++) {
        res = (MTexRes *)MPACK_AT(gTeamSel->chipPack, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    gTeamSel->stagePack = (u32 *)MPACK_AT(gTeamSel->res, 46);
    for (i = 0; i < 38; i++) {
        res = (MTexRes *)MPACK_AT(gTeamSel->stagePack, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    gTeamSel->stageIds = (s32 *)(MPACK_AT(gTeamSel->res, 39) + 0x10);
    gTeamSel->stageCount = TS_PACK_WORD(gTeamSel->res, 39);
    StgGrid_ApplyUnlocks(&gTeamSel->stageCount, gTeamSel->stageIds);
    gTeamSel->bgmIds = (s32 *)(MPACK_AT(gTeamSel->res, 56) + 0x10);
    gTeamSel->bgmCount = TS_PACK_WORD(gTeamSel->res, 56);
    BgmList_ApplyUnlocks(&gTeamSel->bgmCount, gTeamSel->bgmIds);
    /* the last four entries of the list are not offered; the last one kept becomes "random" */
    gTeamSel->bgmCount -= 4;
    gTeamSel->bgmIds[gTeamSel->bgmCount - 1] = TS_BGM_RANDOM;
    gTeamSel->cells[0] = (TsCell *)(MPACK_AT(gTeamSel->res, 42) + 0x10);
    gTeamSel->masterCount[0] = TS_PACK_WORD(gTeamSel->res, 42);
    gTeamSel->cells[1] = gTeamSel->cells[0];
    gTeamSel->masterCount[1] = gTeamSel->masterCount[0];
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        s32 cols = TS_COLS;

        ChrGrid_Build(&gTeamSel->cellCount[i], gTeamSel->grid[i], &gTeamSel->masterCount[i], gTeamSel->cells[i],
                      &gTeamSel->customCount[i], gTeamSel->custom[i]);
        gTeamSel->masterCount[i] = gTeamSel->cellCount[i];
        gTeamSel->cells[i] = gTeamSel->grid[i];
        gTeamSel->rows[i] = gTeamSel->masterCount[i] / cols;
        if (gTeamSel->masterCount[i] % cols) {
            gTeamSel->rows[i]++;
        }
    }
    gTeamSel->faceFile[0] = Heap_Alloc(0x16800, 0x40, 0, 2);
    gTeamSel->faceFile[1] = Heap_Alloc(0x16800, 0x40, 0, 2);
    gTeamSel->faceRes[0] = Heap_Alloc(0x20800, 0x20, 0, 2);
    gTeamSel->faceRes[1] = Heap_Alloc(0x20800, 0x20, 0, 2);
    gTeamSel->stageFile = Heap_Alloc(0x3B800, 0x40, 0, 2);
    gTeamSel->stageRes[0] = Heap_Alloc(0x43000, 0x20, 0, 2);
    gTeamSel->stageRes[1] = Heap_Alloc(0x43000, 0x20, 0, 2);
    gTeamSel->side[0] = &gTeamSel->sideData[0];
    gTeamSel->side[1] = &gTeamSel->sideData[1];
    gTeamSel->stage = &gTeamSel->stageData;
    *(TsTeam *)gTeamSel->side[0] = gTsProgress->team[0];
    *(TsTeam *)gTeamSel->side[1] = gTsProgress->team[1];
    gTeamSel->players = gTsProgress->players;
    gTeamSel->battleType = gTsProgress->battleType;
    gTeamSel->dpLevel = gTsProgress->dpLevel;
    {
        /* two variables are needed to match: the original leaves a dead `li 6` beside the divisor */
        s32 cols = TS_STAGE_COLS;
        s32 rows = TS_STAGE_COLS;

        gTeamSel->stage->col = gTsProgress->stageCell % cols;
        gTeamSel->stage->row = gTsProgress->stageCell / rows;
    }
    for (i = 0; i < gTeamSel->bgmCount; i++) {
        if (gTeamSel->bgmIds[i] == gTsProgress->bgm) {
            gTeamSel->stage->bgmCursor = i;
        }
    }
    gTeamSel->faceState = TEAMSEL_LOAD_IDLE;
    if (gTeamSel->battleType == 2) {
        switch (gTeamSel->dpLevel) {
        case 0:
            gTeamSel->dpMax = 10;
            break;
        case 1:
            gTeamSel->dpMax = 15;
            break;
        case 2:
            gTeamSel->dpMax = 20;
            break;
        }
    }
    /* a remembered cursor on a cell that cannot be chosen any more goes back to column `side`, row 0 */
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        for (j = 0; j < TS_MEMBER_MAX; j++) {
            if (!ChrGrid_IsSelectable(gTeamSel->cells[i], gTeamSel->side[i]->member[j].col +
                                                           gTeamSel->side[i]->member[j].row * TS_COLS)) {
                memset(&gTeamSel->side[i]->member[j], 0, sizeof(TsMember));
                gTeamSel->side[i]->member[j].col = i;
                gTsProgress->team[i].member[j] = gTeamSel->side[i]->member[j];
            }
        }
    }
    if (!StgGrid_IsSelectable(gTeamSel->stageIds, gTsProgress->stageCell)) {
        gTsProgress->stageCell = 0;
        gTeamSel->stage->col = 0;
        gTeamSel->stage->row = 0;
    }
    TeamSel_SetChips(0);
    TeamSel_SetChips(1);
    TeamSel_SetStageChips();
    gTeamSel->side[0]->chara = gTeamSel->side[0]->chip[0][gTeamSel->side[0]->member[gTeamSel->side[0]->cur].col];
    gTeamSel->side[1]->chara = gTeamSel->side[1]->chip[0][gTeamSel->side[1]->member[gTeamSel->side[1]->cur].col];
    gTeamSel->stage->stage = gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + gTeamSel->stage->col];
    gTeamSel->stage->bgm = gTeamSel->stage->bgmCursor;
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        for (j = 0; j < TS_MEMBER_MAX; j++) {
            gTeamSel->side[i]->member[j].chara = -1;
        }
    }
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        File_LoadSync(gTeamSel->side[i]->chara + TS_FACE_FILE, gTeamSel->faceFile[i], 0x16800);
        Sprite_Unpack(gTeamSel->faceFile[i], gTeamSel->faceRes[i], NULL);
        res = gTeamSel->faceRes[i];
        Res_RelocateOffsets(&res, res, res);
        gTeamSel->tex[i ? 22 : 23] = MTEX(res, 0);
        gTeamSel->side[i]->flags |= TEAMSEL_SIDE_FACE_READY;
    }
    File_LoadSync(gTeamSel->stage->stage + TS_STAGE_FILE, gTeamSel->stageFile, 0x3B800);
    Sprite_Unpack(gTeamSel->stageFile, gTeamSel->stageRes[gTeamSel->stageBuf], NULL);
    res = gTeamSel->stageRes[gTeamSel->stageBuf];
    Res_RelocateOffsets(&res, res, res);
    gTeamSel->bg[0] = res;
    gTeamSel->bg[1] = res;
    gTeamSel->stageState = TEAMSEL_LOAD_IDLE;
    gTeamSel->flags |= TEAMSEL_STAGE_READY;
    gTeamSel->bgAlpha[0] = 0x80;
    gTeamSel->bgAlpha[1] = 0;
    gTeamSel->stageBuf = 1;
    if (gTeamSel->bgmIds[gTeamSel->stage->bgm] == TS_BGM_RANDOM) {
        Bgm_Play(Rand_Range(9) + 0x10B1E);
    } else {
        Bgm_Play(gTeamSel->bgmIds[gTeamSel->stage->bgm] + 0x10B16);
    }
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        TextBox_Init(&gTeamSel->nameBox[i], gTeamSel->nameText, i + 1);
        TextBox_SetUnk80(&gTeamSel->nameBox[i], 1);
        TextBox_Init(&gTeamSel->formBox[i], gTeamSel->formText, i + 3);
        TextBox_SetUnk80(&gTeamSel->formBox[i], 1);
    }
}

/* Frees the screen: the windows, the movies, the picture buffers and the work area. */
void TeamSel_Term(void) {
    s32 i;

    IconWin_Term();
    func_00399430();
    ItemPanel_Term(1);
    ItemPanel_Term(0);
    for (i = 0; i < TEAMSEL_FLASH_NUM; i++) {
        Flash_Destroy(&gTeamSel->flash[i]);
    }
    if (gTeamSel->stageRes[1] != NULL) {
        Heap_Free(gTeamSel->stageRes[1]);
        gTeamSel->stageRes[1] = NULL;
    }
    if (gTeamSel->stageRes[0] != NULL) {
        Heap_Free(gTeamSel->stageRes[0]);
        gTeamSel->stageRes[0] = NULL;
    }
    if (gTeamSel->stageFile != NULL) {
        Heap_Free(gTeamSel->stageFile);
        gTeamSel->stageFile = NULL;
    }
    if (gTeamSel->faceRes[1] != NULL) {
        Heap_Free(gTeamSel->faceRes[1]);
        gTeamSel->faceRes[1] = NULL;
    }
    if (gTeamSel->faceRes[0] != NULL) {
        Heap_Free(gTeamSel->faceRes[0]);
        gTeamSel->faceRes[0] = NULL;
    }
    if (gTeamSel->faceFile[1] != NULL) {
        Heap_Free(gTeamSel->faceFile[1]);
        gTeamSel->faceFile[1] = NULL;
    }
    if (gTeamSel->faceFile[0] != NULL) {
        Heap_Free(gTeamSel->faceFile[0]);
        gTeamSel->faceFile[0] = NULL;
    }
    if (gTeamSel->res != NULL) {
        Heap_Free(gTeamSel->res);
        gTeamSel->res = NULL;
    }
    if (gTeamSel != NULL) {
        Heap_Free(gTeamSel);
        gTeamSel = NULL;
    }
}

/* Draws the two stage pictures (cross fade), sets up every clip of the seven movies and draws them and the windows. */
void TeamSel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    char name2[64];
    s32 i;
    s32 j;
    MFlash *flash;
    s32 chara;
    s32 cost;

    for (i = 0; i < 2; i++) {
        if (gTeamSel->flags & TEAMSEL_STAGE_READY) {
            switch (i) {
            case 0:
                gTeamSel->bgAlpha[i] += 5;
                if (gTeamSel->bgAlpha[i] >= 0x80) {
                    gTeamSel->bgAlpha[i] = 0x80;
                }
                break;
            case 1:
                gTeamSel->bgAlpha[i] -= 5;
                if (gTeamSel->bgAlpha[i] < 0) {
                    gTeamSel->bgAlpha[i] = 0;
                }
                break;
            }
        } else {
            /* while the new picture loads only the old one is drawn, opaque */
            if (i == 0) {
                gTeamSel->bgAlpha[i] = 0;
                continue;
            } else if (i == 1) {
                gTeamSel->bgAlpha[i] = 0x80;
            }
        }
        Sprite_DrawPicture(gTeamSel->bg[i], 0, 0, gTeamSel->bgAlpha[i]);
    }
    flash = &gTeamSel->flash[0];
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        Flash_FindLabel(flash, NULL, i ? "mc_face_mask_r" : "mc_face_mask_l", &ref);
        if (gTeamSel->faceMask != 0) {
            Flash_ClipSetFlags(flash, &ref, 0x102, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 0x102, 0);
        }
        if ((gTeamSel->side[i]->flags & (TEAMSEL_SIDE_NO_FACE | TEAMSEL_SIDE_FACE_READY)) == TEAMSEL_SIDE_FACE_READY) {
            gTeamSel->faceAlpha[i] += 0.075f;
            if (gTeamSel->faceAlpha[i] >= 1.0f) {
                gTeamSel->faceAlpha[i] = 1.0f;
            }
        } else {
            gTeamSel->faceAlpha[i] = 0.0f;
        }
        Flash_FindLabel(flash, NULL, i ? "mc_single_chara_r" : "mc_single_chara_l", &ref);
        Flash_ClipSetAlpha(flash, &ref, gTeamSel->faceAlpha[i]);
        Flash_ClipSetFlags(flash, &ref, 0x80, (u8)gTeamSel->faceMask);
        Flash_FindLabel(flash, NULL, i ? "mc_name_text_r" : "mc_name_text_l", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, gTeamSel->side[i]->chara, &gTeamSel->nameBox[i]);
        Flash_FindLabel(flash, NULL, i ? "mc_form_text_r" : "mc_form_text_l", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, gTeamSel->side[i]->chara, &gTeamSel->formBox[i]);
    }
    uv.x0 = 0;
    uv.x1 = 0x200;
    uv.y0 = (gTeamSel->stage->stage % 4) * 0x40;
    uv.y1 = uv.y0 + 0x40;
    uv.unk10 = gTeamSel->stage->stage / 4;
    Flash_FindLabel(flash, NULL, "mc_map_name", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetTex(flash, &ref, uv.unk10);
    if (gTeamSel->battleType == 2) {
        for (i = 0; i < TEAMSEL_SIDES; i++) {
            uv.x0 = (gTeamSel->dpLevel % 2) * 0x40;
            uv.y0 = (gTeamSel->dpLevel / 2) * 0x20;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = uv.y0 + 0x20;
            Flash_FindLabel(flash, NULL, i ? "mc_dp_max_r" : "mc_dp_max_l", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Num_Draw(flash, i ? "mc_dp_total_r_%d" : "mc_dp_total_l_%d", 0, 2, gTeamSel->side[i]->cost, 0x20, 0x40, 1);
        }
    }
    Flash_FindLabel(flash, NULL, "mc_map_mask", &ref);
    if (gTeamSel->stage->mask != 0) {
        Flash_ClipSetFlags(flash, &ref, 0x102, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 0x102, 0);
    }
    for (i = 0; i < 12; i++) {
        sprintf(name, "mc_map_chip_%02d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetFlags(flash, &ref, 0x80, (u8)gTeamSel->stage->mask);
    }
    for (i = 0; i < 2; i++) {
        uv.x0 = i * 0x20;
        uv.x1 = uv.x0 + 0x20;
        uv.y0 = 0x20;
        uv.y1 = 0x40;
        Flash_FindLabel(flash, i ? "mc_yajirusi_down" : "mc_yajirusi_up",
                        i ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    uv.x0 = 0;
    uv.x1 = 0x200;
    uv.y0 = (gTeamSel->bgmIds[gTeamSel->stage->bgmCursor] % 8) * 0x20;
    uv.y1 = uv.y0 + 0x20;
    uv.unk10 = gTeamSel->bgmIds[gTeamSel->stage->bgmCursor] / 8;
    Flash_FindLabel(flash, "mc_bgm_now", "mc_bgm_now_text_off", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetTex(flash, &ref, uv.unk10);
    Flash_FindLabel(flash, "mc_bgm_now", "mc_bgm_now_text_on", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetTex(flash, &ref, uv.unk10);
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        flash = &gTeamSel->flash[5 + i];
        uv.x0 = (i ^ 1) * 0x20;
        uv.x1 = uv.x0 + 0x20;
        uv.y0 = 0;
        uv.y1 = 0x20;
        Flash_FindLabel(flash, NULL, "mc_yajirusi", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        for (j = 0; j < 4; j++) {
            uv.x0 = 0;
            uv.x1 = 0x100;
            uv.y0 = j * 0x20;
            uv.y1 = uv.y0 + 0x20;
            sprintf(name, "mc_custom_plate_%d", j + 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetColor(flash, &ref, j < gTeamSel->plateCount[i] ? 1.0f : 0.3f);
            Flash_FindLabel(flash, name, "mc_custom_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_custom_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if (j == 0) {
                Flash_FindLabel(flash, name, "mc_yajirusi", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_FindLabel(flash, name, "mc_yajirusi", &ref);
                Flash_ClipSetFlags(flash, &ref, 2,
                                   gTeamSel->cells[i][gTeamSel->side[i]->member[gTeamSel->side[i]->cur].col +
                                                      gTeamSel->side[i]->member[gTeamSel->side[i]->cur].row * 7].id !=
                                       TS_RANDOM);
            }
        }
        for (j = 0; j < 4; j++) {
            uv.x0 = (j % 2) * 0x40;
            uv.x1 = uv.x0 + 0x40;
            uv.y0 = (j / 2) * 0x20;
            uv.y1 = uv.y0 + 0x20;
            sprintf(name, "mc_color_plate_%d", j + 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            if (j < gTeamSel->colorCount[i]) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
                switch (gTeamSel->colorCount[i]) {
                case 2:
                    Flash_ClipSetOffset(flash, &ref, 0x2D, 0);
                    break;
                case 3:
                    Flash_ClipSetOffset(flash, &ref, 0x16, 0);
                    break;
                }
                Flash_FindLabel(flash, name, "mc_color_text_off", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_FindLabel(flash, name, "mc_color_text_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
    }
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        u32 n;

        flash = &gTeamSel->flash[3 + i];
        n = 0;
        switch (gTeamSel->players) {
        case 0:
            n = i ? 2 : 0;
            break;
        case 1:
            n = i ? 1 : 0;
            break;
        case 2:
            n = 2;
            break;
        }
        /* the cell of the "1P / 2P / COM" text; the original takes the remainder by subtraction, not `& 1` */
        uv.y0 = (n / 2) * 0x20;
        uv.x0 = (n - (n / 2) * 2) * 0x40;
        uv.y1 = uv.y0 + 0x20;
        uv.x1 = uv.x0 + 0x40;
        Flash_FindLabel(flash, NULL, "mc_plate_text", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (gTeamSel->rows[i] >= 2) {
            for (j = 0; j < 2; j++) {
                uv.x0 = j * 0x20;
                uv.y0 = 0x20;
                uv.x1 = uv.x0 + 0x20;
                uv.y1 = 0x40;
                Flash_FindLabel(flash, j ? "mc_yajirusi_down" : "mc_yajirusi_up",
                                j ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_FindLabel(flash, NULL, j ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
                if (gTeamSel->side[i]->flags & TEAMSEL_SIDE_FLAG40) {
                    Flash_ClipSetFlags(flash, &ref, 2, 0);
                } else if (i == 1 && gTeamSel->players != 1) {
                    if (gTeamSel->side[0]->state != 8) {
                        Flash_ClipSetFlags(flash, &ref, 2, 0);
                    } else {
                        Flash_ClipSetFlags(flash, &ref, 2, 1);
                    }
                } else {
                    Flash_ClipSetFlags(flash, &ref, 2, 1);
                }
            }
        } else {
            for (j = 0; j < 2; j++) {
                Flash_FindLabel(flash, NULL, j ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
        Flash_FindLabel(flash, NULL, "mc_chara_mask", &ref);
        if (gTeamSel->side[i]->mask != 0) {
            Flash_ClipSetFlags(flash, &ref, 0x102, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 0x102, 0);
        }
        for (j = 0; j < 14; j++) {
            s32 cols = TS_COLS;

            sprintf(name, "mc_chara_chip_%03d", j);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 0x80, (u8)gTeamSel->side[i]->mask);
            chara = gTeamSel->side[i]->chip[j / cols][j % cols];
            if (gTeamSel->battleType == 2) {
                if (chara < TS_RANDOM) {
                    cost = ChrTbl_GetCost(chara);
                } else {
                    cost = 0;
                }
                sprintf(name2, "mc_chara_%03d", j);
                Flash_FindLabel(flash, name, name2, &ref);
                if (gTeamSel->side[i]->cost + cost > gTeamSel->dpMax) {
                    Flash_ClipSetColor(flash, &ref, 0.4f);
                } else if (chara < TS_RANDOM) {
                    Flash_ClipSetColor(flash, &ref, TeamSel_IsCharaFree(i, chara) ? 1.0f : 0.4f);
                } else {
                    Flash_ClipSetColor(flash, &ref, 1.0f);
                }
                sprintf(name2, "mc_dp_num_%03d", j);
                Flash_FindLabel(flash, name, name2, &ref);
                if (chara < TS_RANDOM) {
                    uv.x0 = (cost % 4) * 0x20;
                    uv.x1 = uv.x0 + 0x20;
                    uv.y0 = (cost / 4) * 0x20;
                    uv.y1 = uv.y0 + 0x20;
                    Flash_ClipSetUv(flash, &ref, &uv);
                    Flash_ClipSetFlags(flash, &ref, 2, 1);
                } else {
                    Flash_ClipSetFlags(flash, &ref, 2, 0);
                }
            } else {
                sprintf(name2, "mc_chara_%03d", j);
                Flash_FindLabel(flash, name, name2, &ref);
                if (chara < TS_RANDOM) {
                    Flash_ClipSetColor(flash, &ref, TeamSel_IsCharaFree(i, chara) ? 1.0f : 0.4f);
                } else {
                    Flash_ClipSetColor(flash, &ref, 1.0f);
                }
            }
        }
    }
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        flash = &gTeamSel->flash[1 + i];
        for (j = 0; j < TS_MEMBER_MAX; j++) {
            sprintf(name, "mc_team_%d", j);
            if (i == 0) {
                uv.x0 = (j % 4) * 0x40;
                uv.x1 = uv.x0 + 0x40;
                uv.y0 = (j / 4) * 0x48;
                uv.y1 = uv.y0 + 0x48;
            } else {
                uv.x0 = ((j + 5) % 4) * 0x40;
                uv.x1 = uv.x0 + 0x40;
                uv.y0 = ((j + 5) / 4) * 0x48;
                uv.y1 = uv.y0 + 0x48;
            }
            sprintf(name2, "mc_team_plate_%d", j);
            Flash_FindLabel(flash, name, name2, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            sprintf(name2, "mc_dp_num_%03d", j);
            Flash_FindLabel(flash, name, name2, &ref);
            if (j < gTeamSel->side[i]->memberCount) {
                cost = ChrTbl_GetCost(gTeamSel->side[i]->member[j].chara);
                uv.x0 = (cost % 4) * 0x20;
                uv.x1 = uv.x0 + 0x20;
                uv.y0 = (cost / 4) * 0x20;
                uv.y1 = uv.y0 + 0x20;
                Flash_ClipSetFlags(flash, &ref, 2, 1);
                Flash_ClipSetUv(flash, &ref, &uv);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            sprintf(name2, "mc_dp_plate_%03d", j);
            Flash_FindLabel(flash, name, name2, &ref);
            if (j < gTeamSel->side[i]->memberCount) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
    }
    for (i = 0; i < TEAMSEL_FLASH_NUM; i++) {
        Flash_Draw(&gTeamSel->flash[i]);
    }
    Font_FlushAll();
    IconWin_Draw();
    ItemPanel_Draw(0);
    ItemPanel_Draw(1);
    func_00399478(gTeamSel->help);
}
