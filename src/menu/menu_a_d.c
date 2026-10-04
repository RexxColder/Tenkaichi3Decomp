#include "common.h"
#include "menu/menu_a.h"
#include "sys/save.h"

/*
 * ModeMenu, from 0x338020 (this file ends at 0x339610; the object goes on in the next chunk, which has
 * func_0033A360, func_0033A798 and func_0033A800). The sub menu of one game mode (gProgress->subMenu, 0..7):
 * a horizontal list of up to 16 items of which three are visible, a guide character with voice, a large picture
 * of the current item that is loaded from disc in the background, and three lines of description.
 */

extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_SetUnk50(MTextBox *box, s32 value);
extern void TextBox_SetRect(MTextBox *box, s32 a, s32 b, s32 c, s32 d);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);

extern void func_0033A360(u32 *fileRes);
extern void func_0033A798(void);
extern void func_0033A800(void);

#define MODEMENU_BGM 0x10B1C

/* Index of an item's first line in a text shared by the sub menus: items are numbered through all of them, and
   lines 20..23 are rotated by one. */
s32 ModeMenu_GetLine(s32 menu, s32 item) {
    s32 count[8] = { 3, 4, 7, 5, 16, 5, 4, 4 };
    s32 line = 0;
    s32 i;

    for (i = 0; i < menu; i++) {
        line += count[i];
    }
    line += item;
    switch (line) {
    case 20:
        line = 21;
        break;
    case 21:
        line = 22;
        break;
    case 22:
        line = 23;
        break;
    case 23:
        line = 20;
        break;
    }
    return line;
}

/* One step of the background loader of the current item's large picture. */
void ModeMenu_UpdateImage(void) {
    MTexRes *res;

    switch (gModeMenu->loadState) {
    case MODEMENU_LOAD_ABORT:
        gModeMenu->loadState = MODEMENU_LOAD_RESTART;
        break;
    case MODEMENU_LOAD_RESTART:
        if (gModeMenu->flags & MODEMENU_IMAGE_CHANGE) {
            gModeMenu->flags ^= MODEMENU_IMAGE_CHANGE;
        }
        gModeMenu->loadState = MODEMENU_LOAD_REQUEST;
        break;
    case MODEMENU_LOAD_REQUEST:
        File_CancelRequests();
        File_Request(gModeMenu->imageBase + gModeMenu->items[gModeMenu->cursor], gModeMenu->imageFile, 0x1C000);
        gModeMenu->loadState = MODEMENU_LOAD_READ;
        break;
    case MODEMENU_LOAD_READ:
        if (File_UpdateRequests()) {
            gModeMenu->loadState = MODEMENU_LOAD_UNPACK;
        }
        break;
    case MODEMENU_LOAD_UNPACK:
        Sprite_Unpack(gModeMenu->imageFile, gModeMenu->imageRes, NULL);
        res = gModeMenu->imageRes;
        Res_RelocateOffsets(&res, res, res);
        gModeMenu->tex[9] = MTEX(res, 0);
        gModeMenu->flags |= MODEMENU_IMAGE_READY;
        gModeMenu->loadState = MODEMENU_LOAD_SHOWN;
        break;
    case MODEMENU_LOAD_SHOWN:
        if (gModeMenu->flags & MODEMENU_IMAGE_CHANGE) {
            gModeMenu->tex[9] = NULL;
            gModeMenu->loadState = MODEMENU_LOAD_RESTART;
        }
        break;
    }
}

/* The current item changed: hide the picture, or abort a load in progress, and ask for the new one. */
void ModeMenu_ChangeImage(void) {
    if (gModeMenu->flags & MODEMENU_IMAGE_READY) {
        gModeMenu->flags ^= MODEMENU_IMAGE_READY;
    } else {
        gModeMenu->loadState = MODEMENU_LOAD_ABORT;
    }
    gModeMenu->flags |= MODEMENU_IMAGE_CHANGE;
}

#define MD_RES(pack, n) \
    res = (MTexRes *)MPACK_AT(pack, n); \
    Res_RelocateOffsets(&res, res, res)

#define MD_ICONS(a, b, c) \
    gModeMenu->tex[a] = MTEX(res, 0); \
    gModeMenu->tex[b] = MTEX(res, 1); \
    gModeMenu->tex[c] = MTEX(res, 3)

#define MD_SETUP(max, voice, v10C, image) \
    gModeMenu->itemMax = (max); \
    gModeMenu->voiceBase = (voice); \
    gModeMenu->unk10C = (v10C); \
    gModeMenu->imageBase = (image)

/* Loads the screen (section `section` of archive 2 and the sub menu's own file), builds the list of unlocked
   items from the save and starts the music. */
void ModeMenu_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gModeMenu = Heap_Alloc(0x33C, 0x20, 0, 2);
    memset(gModeMenu, 0, 0x33C);
    gModeMenu->pack = (u32 *)MPACK_AT(gMenuArc2, section);
    gModeMenu->res = Sprite_Unpack(gModeMenu->pack, NULL, NULL);
    gModeMenu->file = File_LoadSync(gProgress->baseFile + gProgress->subMenu + 0x10, NULL, 0);
    gModeMenu->fileRes = Sprite_Unpack(gModeMenu->file, NULL, NULL);
    func_0033A360(gModeMenu->fileRes);
    MD_RES(gModeMenu->fileRes, 7);
    switch (gProgress->subMenu) {
    case 0:
        MD_ICONS(10, 11, 12);
        break;
    case 1:
        MD_ICONS(13, 14, 15);
        break;
    case 2:
        MD_ICONS(16, 17, 18);
        break;
    case 3:
        MD_ICONS(19, 21, 20);
        break;
    case 4:
        MD_ICONS(22, 23, 24);
        break;
    case 5:
        MD_ICONS(25, 26, 27);
        break;
    case 6:
        MD_ICONS(28, 29, 30);
        break;
    case 7:
        MD_ICONS(31, 32, 33);
        break;
    }
    MD_RES(gModeMenu->res, 1);
    gModeMenu->tex[8] = MTEX(res, 0);
    gModeMenu->tex[7] = MTEX(res, 1);
    gModeMenu->tex[34] = MTEX(res, 2);
    gModeMenu->tex[38] = MTEX(res, 4);
    gModeMenu->tex[0] = MTEX(res, 5);
    gModeMenu->tex[6] = MTEX(res, 6);
    gModeMenu->tex[5] = MTEX(res, 7);
    gModeMenu->tex[3] = MTEX(res, 8);
    MD_RES(gModeMenu->res, 2);
    gModeMenu->tex[2] = MTEX(res, 0);
    MD_RES(gModeMenu->fileRes, 2);
    gModeMenu->tex[4] = MTEX(res, 0);
    gModeMenu->tex[36] = MTEX(res, 1);
    MD_RES(gModeMenu->fileRes, 3);
    gModeMenu->tex[37] = MTEX(res, 0);
    gModeMenu->tex[1] = MTEX(res, 1);
    MD_RES(gModeMenu->res, 7);
    gModeMenu->tex[35] = MTEX(res, 1);
    Flash_Create(&gModeMenu->flash[0], MPACK_AT(gModeMenu->res, 3), gModeMenu->tex);
    Flash_Play(&gModeMenu->flash[0], 1);
    gModeMenu->msgText = MPACK_AT(gModeMenu->fileRes, 4);
    gModeMenu->subtitles = MPACK_AT(gModeMenu->fileRes, 5);
    MsgWin_Init(MPACK_AT(gModeMenu->res, 8), gModeMenu->msgText, 1, 0);
    gModeMenu->text = MPACK_AT(gModeMenu->res, 6);

    switch (gProgress->subMenu) {
    case 0:
        MD_SETUP(3, 0x8537, 0x13, 0x3CE);
        break;
    case 1:
        MD_SETUP(4, 0x8423, 0xC, 0x3D1);
        break;
    case 2:
        MD_SETUP(7, 0x83B4, 0x12, 0x3D5);
        break;
    case 3:
        MD_SETUP(5, 0x83DB, 0x15, 0x3DC);
        break;
    case 4:
        MD_SETUP(16, 0x8484, 0x13, 0x3E1);
        break;
    case 5:
        MD_SETUP(5, 0x8440, 0x14, 0x3F1);
        break;
    case 6:
        MD_SETUP(4, 0x8402, 0x10, 0x3F6);
        break;
    case 7:
        MD_SETUP(4, 0x8466, 0xE, 0x3FA);
        break;
    }

    for (i = 0; i < gModeMenu->itemMax; i++) {
        SaveSlot *slot = &gSaveData->slot[gProgress->subMenu];

        if (slot->val[0] & (s32)(1U << i)) {
            gModeMenu->items[gModeMenu->itemCount++] = i;
        }
    }
    for (i = gModeMenu->itemCount; i < MODEMENU_ROWS; i++) {
        gModeMenu->items[gModeMenu->itemCount++] = gModeMenu->itemMax;
    }
    gModeMenu->descr = (ModeMenuText *)MPACK_AT(gModeMenu->fileRes, 6);
    if (gProgress->subMenu == 7) {
        gModeMenu->descr->count = 2;
    }
    if (gProgress->prevMode == 10 || gProgress->prevMode == 8) {
        for (i = 0; i < gModeMenu->itemCount; i++) {
            if (gModeMenu->items[i] == gProgress->subMenuItem) {
                gModeMenu->cursor = i;
                break;
            }
        }
    }
    gModeMenu->top = gModeMenu->cursor;
    gModeMenu->bottom = gModeMenu->top + 2;
    while (gModeMenu->bottom >= gModeMenu->itemCount) {
        gModeMenu->top--;
        gModeMenu->bottom--;
    }
    gModeMenu->imageFile = Heap_Alloc(0x1C000, 0x40, 0, 2);
    gModeMenu->imageRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    gModeMenu->loadState = MODEMENU_LOAD_REQUEST;
    gModeMenu->voiceLine = -1;
    gModeMenu->blink = Rand_Range(0x20);
    for (i = 0; i < 3; i++) {
        TextBox_Init(&gModeMenu->box[i], gModeMenu->text, 0);
        TextBox_SetUnk80(&gModeMenu->box[i], 1);
        TextBox_SetUnk50(&gModeMenu->box[i], 1);
        TextBox_SetRect(&gModeMenu->box[i], 0, 0x200, 0, 0x1A1);
    }

    switch (gProgress->subMenu) {
    case 0:
        Bgm_Play(MODEMENU_BGM);
        StreamSe_PlayDefault(0, 0x10BDC);
        break;
    case 1:
        Bgm_Play(MODEMENU_BGM);
        StreamSe_PlayDefault(0, 0x10BDD);
        break;
    case 2:
        Bgm_Play(MODEMENU_BGM);
        StreamSe_PlayDefault(0, 0x10BDE);
        break;
    case 3:
    case 4:
        Bgm_Play(MODEMENU_BGM);
        break;
    case 5:
        Bgm_Play(0x10B1E);
        break;
    case 6:
        Bgm_Play(0x10B28);
        break;
    case 7:
        Bgm_Play(MODEMENU_BGM);
        break;
    }
}

#define MD_FREE(p) \
    if ((p) != NULL) { \
        Heap_Free(p); \
        (p) = NULL; \
    }

/* Frees the screen. */
void ModeMenu_Term(void) {
    s32 i;

    func_0033A798();
    MsgWin_Term();
    for (i = 0; i < MODEMENU_FLASH_NUM; i++) {
        Flash_Destroy(&gModeMenu->flash[i]);
    }
    MD_FREE(gModeMenu->imageRes);
    MD_FREE(gModeMenu->imageFile);
    MD_FREE(gModeMenu->fileRes);
    MD_FREE(gModeMenu->file);
    MD_FREE(gModeMenu->res);
    MD_FREE(gModeMenu);
}

/* An item's bit in one of the sub menu's three save words. */
#define MD_SAVE_BIT(n, item) (gSaveData->slot[gProgress->subMenu].val[n] & (s32)(1U << (item)))

/* Sets up every clip of the movie for this frame and draws it and the message window. */
void ModeMenu_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    char sub[64];
    s32 i;
    MFlash *flash;

    func_0033A800();
    flash = &gModeMenu->flash[0];
    sprintf(name, "mc_guide_%02d", gProgress->subMenu);
    sprintf(sub, "mc_guide_%02d_eye", gProgress->subMenu);
    Flash_FindLabel(flash, name, sub, &ref);
    FlashAnim_Blink(flash, &ref, &gModeMenu->blink, 0);
    sprintf(sub, "mc_guide_%02d_mouth", gProgress->subMenu);
    Flash_FindLabel(flash, name, sub, &ref);
    FlashAnim_Talk(flash, &ref, &gModeMenu->talk, 0);

    if (gModeMenu->flags & MODEMENU_IMAGE_READY) {
        gModeMenu->imageAlpha += 0.075f;
        if (gModeMenu->imageAlpha >= 1.0f) {
            gModeMenu->imageAlpha = 1.0f;
        }
    } else {
        gModeMenu->imageAlpha = 0.0f;
    }
    Flash_FindLabel(flash, NULL, "mc_image_l_1", &ref);
    Flash_ClipSetFlags(flash, &ref, 0x80, 1);
    Flash_FindLabel(flash, NULL, "mc_battle_mask", &ref);
    Flash_ClipSetAlpha(flash, &ref, gModeMenu->imageAlpha);
    Flash_ClipSetFlags(flash, &ref, 0x100, 1);

    uv.x0 = 0;
    uv.y0 = (gModeMenu->items[gModeMenu->cursor] % 4) * 0x40;
    uv.x1 = 0x200;
    uv.y1 = uv.y0 + 0x40;
    Flash_FindLabel(flash, NULL, "mc_battle_text_1", &ref);
    Flash_ClipSetAlpha(flash, &ref, gModeMenu->imageAlpha);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetTex(flash, &ref, gModeMenu->items[gModeMenu->cursor] / 4);

    Flash_FindLabel(flash, NULL, "mc_clear", &ref);
    if (MD_SAVE_BIT(1, gModeMenu->items[gModeMenu->cursor])) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
        Flash_ClipSetAlpha(flash, &ref, gModeMenu->imageAlpha);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }

    for (i = 0; i < 2; i++) {
        uv.y0 = 0;
        uv.x0 = i * 0x20;
        uv.x1 = uv.x0 + 0x20;
        uv.y1 = 0x20;
        switch (i) {
        case 0:
            Flash_FindLabel(flash, "mc_yajirusi_left", "mc_yajirusi_icon_left", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, gModeMenu->top != 0);
            break;
        case 1:
            Flash_FindLabel(flash, "mc_yajirusi_right", "mc_yajirusi_icon_right", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, gModeMenu->bottom != gModeMenu->itemCount - 1);
            break;
        }
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    for (i = 0; i < MODEMENU_ROWS; i++) {
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_icon_new", &ref);
        if (gModeMenu->items[gModeMenu->top + i] != gModeMenu->itemMax &&
            MD_SAVE_BIT(2, gModeMenu->items[gModeMenu->top + i])) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        Flash_FindLabel(flash, name, "mc_image_s", &ref);
        Flash_ClipSetTex(flash, &ref, gModeMenu->items[gModeMenu->top + i]);
    }
    for (i = 0; i < 2; i++) {
        sprintf(name, "mc_menu_plate_%d", i + 4);
        Flash_FindLabel(flash, name, "mc_icon_new", &ref);
        if (gModeMenu->items[gModeMenu->extra] != gModeMenu->itemMax &&
            MD_SAVE_BIT(2, gModeMenu->items[gModeMenu->extra])) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        Flash_FindLabel(flash, name, "mc_image_s", &ref);
        Flash_ClipSetTex(flash, &ref, gModeMenu->items[gModeMenu->extra]);
    }

    for (i = 0; i < 3; i++) {
        sprintf(name, "mc_episode_text_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (i >= gModeMenu->descr[gModeMenu->items[gModeMenu->cursor]].count) {
            TextBox_AttachLine(flash, &ref, 0, -(s32)gModeMenu->scroll, -1, &gModeMenu->box[i]);
        } else {
            TextBox_AttachLine(flash, &ref, 0, -(s32)gModeMenu->scroll,
                               gModeMenu->descr[gModeMenu->items[gModeMenu->cursor]].line + i, &gModeMenu->box[i]);
        }
    }
    Flash_FindLabel(flash, NULL, "mc_episode_next", &ref);
    Flash_ClipSetFlags(flash, &ref, 2, (gModeMenu->flags >> 5) & 1);

    for (i = 0; i < MODEMENU_FLASH_NUM; i++) {
        Flash_Draw(&gModeMenu->flash[i]);
    }
    Font_FlushAll();
    MsgWin_Draw(0, 0, gModeMenu->voiceLine);
}
