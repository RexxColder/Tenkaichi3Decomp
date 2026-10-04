#include "common.h"
#include "menu/menu_z.h"

/*
 * Menu overlay DBZP.BIN, 0x3A7D98..0x3A9850: tail of the DcList object, the list of saved custom characters of
 * the Data Center (progress mode 55). The head of the object (the list itself, its menu and most of the drawing)
 * is in the previous chunk. The work structure is a local variable of DcList_Run, not a heap block.
 */

/* Starts a line of the guide and shows its subtitle. */
static inline void DcList_Say(DcList *list, s32 line) {
    Voice_PlayWithSubtitle(list->subtitles, DC_VOICE_BASE, line);
    list->voiceLine = line;
}

/* Sets the texture rectangle of a clip found by name (each use has its own MFlashRef on the stack). */
static inline void DcList_SetUv(MFlash *flash, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Texture rectangle of one cell of a sheet. */
static inline void DcList_SetCell(MFlashUv *uv, s32 col, s32 row, s32 w, s32 h) {
    uv->x0 = col * w;
    uv->y0 = row * h;
    uv->x1 = uv->x0 + w;
    uv->y1 = uv->y0 + h;
}

/* Keeps the item page's cursor in 0..7 (a ring). */
void DcList_WrapItemCursor(s32 *cursor) {
    s32 max = DCLIST_ITEM_ROWS - 1;

    if (*cursor < 0) {
        *cursor = max;
    } else if (*cursor > max) {
        *cursor = 0;
    }
}

/* Whether the cursor skips a row of the item page: it holds no item, and it is not the last row. */
s32 DcList_IsRowSkipped(u16 *items, s32 row) {
    s32 ret = 0;

    items += row;
    if ((u16)(*items - 1) >= SAVE_ITEM_COUNT) {
        ret = row != DCLIST_ITEM_ROWS - 1;
    }
    return ret;
}

/* Puts the character's name and form name on the large plates of the item page. */
void DcList_DrawNames(DcListView *view, s32 chara) {
    MFlashRef ref;
    MFlashUv uv;
    MFlash *flash = &view->flash[1];

    Flash_FindLabel(flash, NULL, "mc_name_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &view->nameBoxL);
    uv.x0 = 0;
    uv.y0 = 0;
    uv.x1 = 0x100;
    uv.y1 = 0x20;
    DcList_SetUv(flash, NULL, "mc_name_plate_l", &uv);
    Flash_FindLabel(flash, NULL, "mc_form_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &view->formBoxL);
}

/* Lights or dims a row of the item page. */
void DcList_LightItemRow(DcListView *view, s32 row, s32 on) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash;

    sprintf(name, "mc_list_plate_%d", row + 1);
    flash = &view->flash[1];
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Fills the eight rows of the item page from the current record: name, cost icon, kind icon. */
void DcList_DrawItems(DcList *list, s32 cursor) {
    MFlash *flash = &list->view.flash[1];
    MFlashRef ref;
    MFlashUv uv;
    char name[0x100];
    s32 i;

    for (i = 0; i < DCLIST_ITEM_ROWS; i++) {
        u16 item = list->status.rec.item[i];
        s32 slots = list->itemTbl[item - 1].slots;

        sprintf(name, "mc_list_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
        if (item != 0) {
            TextBox_AttachLine(flash, &ref, 0, 0, item - 1, &list->view.itemBox[i]);
        }
        Flash_FindLabel(flash, name, "mc_icon_custom", &ref);
        if (i == cursor) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        uv.x0 = 0x40;
        uv.y0 = 0;
        uv.x1 = 0x80;
        uv.y1 = 0x40;
        Flash_FindLabel(flash, name, "mc_icon_custom", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (item != 0) {
            if (slots != 0) {
                DcList_SetCell(&uv, list->itemTbl[item - 1].slots - 1, ItemTbl_GetClass(item - 1, list->itemTbl), 0x20, 0x2A);
                DcList_SetUv(flash, name, "mc_icon_item_cost", &uv);
                Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            uv.x0 = list->itemTbl[item - 1].type << 6;
            uv.y0 = 0;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = 0x40;
            DcList_SetUv(flash, name, "mc_icon_item_potara", &uv);
            Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
            Flash_ClipSetTex(flash, &ref, 1);
            Flash_FindLabel(flash, name, "mc_list_plate_on", &ref);
            Flash_ClipSetTex(flash, &ref, 0);
        } else {
            Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
            Flash_ClipSetTex(flash, &ref, 5);
            Flash_FindLabel(flash, name, "mc_list_plate_on", &ref);
            Flash_ClipSetTex(flash, &ref, 3);
        }
        if (i == DCLIST_ITEM_ROWS - 1) {
            Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
            Flash_ClipSetTex(flash, &ref, 4);
            Flash_FindLabel(flash, name, "mc_list_plate_on", &ref);
            Flash_ClipSetTex(flash, &ref, 3);
        }
    }
}

/* Cuts the two "limit" numbers of the item page from their sheets. */
void DcList_DrawLimitNums(DcList *list) {
    MFlashUv uv;
    MFlash *flash = &list->view.flash[1];

    DcList_SetCell(&uv, 2, 2, 0x40, 0x40);
    DcList_SetUv(flash, NULL, "mc_ability_limit_big_num", &uv);
    DcList_SetCell(&uv, 2, 2, 0x20, 0x20);
    DcList_SetUv(flash, NULL, "mc_ability_limit_s_num", &uv);
}

/* The item id (1-based, 0 = none) on the row under the item page's cursor. */
u16 DcList_GetCursorItem(DcList *list) {
    return list->status.rec.item[list->itemCursor];
}

/* Plays the "chosen" animation of the row under the cursor. */
void DcList_FlashItemRow(DcList *list) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash;

    sprintf(name, "mc_list_plate_%d", list->itemCursor + 1);
    flash = &list->view.flash[1];
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_ok");
}

/* Fills the item page: names, the eight rows, the limit numbers. */
void DcList_DrawItemPage(DcList *list) {
    DcList_DrawNames(&list->view, list->status.rec.chara);
    DcList_DrawItems(list, list->itemCursor);
    DcList_DrawLimitNums(list);
}

/* State 2, the item page: up / down move over the rows that hold an item, confirm opens the details. */
void DcList_InputItems(DcList *list) {
    if (gPad[0].gamePressed & ZPAD_OK) {
        DcList_FlashItemRow(list);
        if (DcList_GetCursorItem(list) != 0) {
            ItemHelp_Open();
            list->state = DCLIST_ST_ITEM_HELP;
            Snd_PlaySe(1, 1);
        } else {
            Snd_PlaySe(1, 7);
        }
    } else if (gPad[0].gamePressed & ZPAD_CANCEL) {
        list->state = DCLIST_ST_MENU;
        Flash_GotoLabel(&list->view.flash[1], "fl_evo_cansel", 1);
        DcList_LightItemRow(&list->view, list->itemCursor, 0);
        list->itemCursor = 0;
        DcList_LightItemRow(&list->view, list->itemCursor, 1);
        Snd_PlaySe(1, 2);
    } else if (gPad[0].gameRepeat & ZPAD_UP) {
        DcList_LightItemRow(&list->view, list->itemCursor, 0);
        do {
            list->itemCursor--;
            DcList_WrapItemCursor(&list->itemCursor);
        } while (DcList_IsRowSkipped(list->status.rec.item, list->itemCursor));
        DcList_LightItemRow(&list->view, list->itemCursor, 1);
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gameRepeat & ZPAD_DOWN) {
        DcList_LightItemRow(&list->view, list->itemCursor, 0);
        do {
            list->itemCursor++;
            DcList_WrapItemCursor(&list->itemCursor);
        } while (DcList_IsRowSkipped(list->status.rec.item, list->itemCursor));
        DcList_LightItemRow(&list->view, list->itemCursor, 1);
        Snd_PlaySe(1, 0);
    }
}

/* Draws the item details page for the item under the cursor. */
void DcList_DrawItemHelp(DcList *list) {
    s32 item = DcList_GetCursorItem(list) - 1;

    if (item < 0) {
        item = 0;
    }
    ItemHelp_Draw(item);
}

/* State 3, the item details page: confirm or cancel closes it. */
void DcList_InputItemHelp(DcList *list) {
    if (DcList_GetCursorItem(list) != 0) {
        if (gPad[0].gamePressed & ZPAD_OK) {
            list->state = DCLIST_ST_ITEMS;
            ItemHelp_Close();
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & ZPAD_CANCEL) {
            list->state = DCLIST_ST_ITEMS;
            ItemHelp_Close();
            Snd_PlaySe(1, 2);
        }
    }
}

/* Draws the window that shows the character's password. */
void DcList_DrawPassword(DcList *list) {
    PassWin_Draw();
}

/* State 4, the password window: confirm or cancel closes it. */
void DcList_InputPassword(DcList *list) {
    if ((gPad[0].gamePressed & ZPAD_OK) || (gPad[0].gamePressed & ZPAD_CANCEL)) {
        PassWin_Close();
        list->state = DCLIST_ST_MENU;
        Snd_PlaySe(1, 2);
    }
}

/* Empties the record under the cursor, in the list's copy and in the save, and marks the save as changed. */
void DcList_DeleteRec(DcList *list) {
    s32 idx = DcChars_GetCursor(&list->list);

    ZSAVE->rec[idx].chara = list->list.rec[DcChars_GetCursor(&list->list)].chara = -1;
    ZPROG->flags |= ZPROG_DIRTY;
}

/* Draws the confirmation dialog. */
void DcList_DrawDialog(DcList *list) {
    Dialog_Draw(0);
}

/* State 5, "delete this character?": yes deletes the record, then the dialog closes into the next state. */
void DcList_InputDialog(DcList *list) {
    s32 result;

    if (Dialog_IsClosed()) {
        list->state = list->nextState;
        if (list->state == DCLIST_ST_DELETED) {
            Flash_GotoLabel(&list->view.flash[0], "fl_chara_details_out", 1);
            DcList_Say(list, 0x1B);
            MsgWin_Open();
        }
    }
    result = Dialog_Input(0);
    if (result == 1) {
        DcList_DeleteRec(list);
        list->nextState = DCLIST_ST_DELETED;
        Dialog_Start(1);
        Dialog_SetChoices(0);
    } else if (result == -2) {
        list->nextState = DCLIST_ST_MENU;
        Dialog_Start(1);
        Dialog_SetChoices(0);
    }
}

/* State 6: the guide's line after a deletion; confirm (or its end) returns to the list. */
void DcList_InputDeleted(DcList *list) {
    if ((gPad[0].gamePressed & ZPAD_OK) || Voice_GetStat(0) == MVOICE_IDLE) {
        list->state = DCLIST_ST_LIST;
        if (gPad[0].gamePressed & ZPAD_OK) {
            Snd_PlaySe(1, 1);
        }
        DcList_Say(list, 0x1A);
    }
}

/* Asks for the large picture of a character (the file is read in the background). */
static inline void DcList_RequestFace(DcList *list, ZStatus *status) {
    File_CancelRequests();
    File_Request(status->rec.chara + DCLIST_FACE_FILE, list->faceFile, DCLIST_FACE_SIZE);
}

/* Loads the large picture of the character under the cursor in the background and fades it in. */
void DcList_UpdateFace(DcList *list) {
    MTexRes *res;
    s32 flags;

    if (list->flags & DCLIST_FACE_CHANGE) {
        list->status.rec = DcChars_GetRec(&list->list);
        DcList_RequestFace(list, &list->status);
        flags = list->flags;
        list->faceAlpha = 0.0f;
        list->flags = (flags ^ DCLIST_FACE_CHANGE) | DCLIST_FACE_LOADING;
        if (list->flags & DCLIST_FACE_READY) {
            list->flags ^= DCLIST_FACE_READY;
        }
    } else if (list->flags & DCLIST_FACE_LOADING) {
        if (File_UpdateRequests()) {
            res = NULL;
            Sprite_Unpack(list->faceFile, list->faceRes, NULL);
            res = list->faceRes;
            Res_RelocateOffsets(&res, res, res);
            list->view.tex0[7] = res->tex;
            list->flags = (list->flags ^ DCLIST_FACE_LOADING) | DCLIST_FACE_READY;
        }
    }
    if (list->flags & DCLIST_FACE_READY) {
        if (list->faceAlpha < 1.0f) {
            list->faceAlpha += 0.05f;
        }
    }
}

/* Advances the two movies. */
void DcList_Update(DcList *list) {
    s32 i;

    for (i = 0; i < DCLIST_FLASH_NUM; i++) {
        Flash_Advance(&list->view.flash[i]);
    }
}

#define DCLIST_RES(n) \
    res = (MTexRes *)MPACK_AT(list->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Unpacks the screen's section of archive 8, builds the two movies, the text boxes and the shared windows. */
void DcList_Init(DcList *list) {
    MTexRes *res = NULL;
    s32 i;
    s32 j;

    list->pack = MPACK_AT(gMenuArc8, list->section);
    list->res = Sprite_Unpack(list->pack, NULL, NULL);
    list->faceFile = Heap_Alloc(DCLIST_FACE_SIZE, 0x40, 0, 2);
    list->faceRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    DCLIST_RES(1);
    list->view.bg = res;
    File_LoadSync(DcChars_GetCursor(&list->list) + DCLIST_FACE_FILE, list->faceFile, DCLIST_FACE_SIZE);
    Sprite_Unpack(list->faceFile, list->faceRes, NULL);
    DCLIST_RES(19);
    list->view.tex0[0] = MTEX(res, 0);
    list->view.tex0[1] = MTEX(res, 1);
    list->view.tex0[2] = MTEX(res, 2);
    list->view.tex0[3] = MTEX(res, 3);
    list->view.tex0[4] = MTEX(res, 4);
    list->view.tex0[5] = MTEX(res, 5);
    list->view.tex0[6] = MTEX(res, 6);
    DCLIST_RES(20);
    list->view.tex0[13] = MTEX(res, 0);
    DCLIST_RES(3);
    list->view.tex0[8] = MTEX(res, 0);
    list->view.tex0[9] = MTEX(res, 1);
    list->view.tex0[10] = MTEX(res, 2);
    list->view.tex0[11] = MTEX(res, 3);
    list->view.tex0[15] = MTEX(res, 4);
    list->view.tex0[17] = MTEX(res, 5);
    list->view.tex0[18] = MTEX(res, 6);
    list->view.tex0[20] = MTEX(res, 7);
    list->view.tex0[21] = MTEX(res, 8);
    list->view.tex0[24] = MTEX(res, 9);
    list->view.tex0[25] = MTEX(res, 10);
    list->view.tex0[27] = MTEX(res, 11);
    list->view.tex0[28] = MTEX(res, 12);
    list->view.tex0[29] = MTEX(res, 13);
    list->view.tex0[32] = MTEX(res, 14);
    list->view.tex0[33] = MTEX(res, 15);
    list->view.tex0[34] = MTEX(res, 16);
    DCLIST_RES(4);
    list->view.tex0[12] = MTEX(res, 0);
    list->view.tex0[14] = MTEX(res, 1);
    list->view.tex0[16] = MTEX(res, 2);
    list->view.tex0[19] = MTEX(res, 3);
    list->view.tex0[26] = MTEX(res, 4);
    DCLIST_RES(2);
    list->view.tex0[35] = MTEX(res, 0);
    list->view.tex0[36] = MTEX(res, 1);
    list->view.tex0[37] = MTEX(res, 3);
    list->view.tex0[7] = NULL;
    list->view.tex0[22] = NULL;
    list->view.tex0[23] = NULL;
    list->view.tex0[30] = NULL;
    list->view.tex0[31] = NULL;
    Flash_Create(&list->view.flash[0], MPACK_AT(list->res, 5), list->view.tex0);
    Flash_Play(&list->view.flash[0], 1);
    DCLIST_RES(7);
    list->view.tex1[0] = MTEX(res, 0);
    list->view.tex1[1] = MTEX(res, 1);
    list->view.tex1[2] = MTEX(res, 2);
    list->view.tex1[4] = MTEX(res, 3);
    list->view.tex1[5] = MTEX(res, 4);
    list->view.tex1[6] = MTEX(res, 5);
    list->view.tex1[10] = MTEX(res, 6);
    list->view.tex1[11] = MTEX(res, 7);
    list->view.tex1[12] = MTEX(res, 8);
    list->view.tex1[13] = MTEX(res, 9);
    list->view.tex1[14] = MTEX(res, 10);
    DCLIST_RES(8);
    list->view.tex1[3] = MTEX(res, 0);
    DCLIST_RES(6);
    list->view.tex1[7] = MTEX(res, 0);
    list->view.tex1[8] = MTEX(res, 0);
    list->view.tex1[9] = NULL;
    list->view.tex1[15] = NULL;
    list->view.tex1[16] = NULL;
    Flash_Create(&list->view.flash[1], MPACK_AT(list->res, 9), list->view.tex1);
    Flash_Play(&list->view.flash[1], 1);
    list->nameText = MPACK_AT(list->res, 17);
    list->formText = MPACK_AT(list->res, 18);
    for (i = 0; i < 4; i++) {
        TextBox_Init(&list->view.nameBox[i], list->nameText, 2);
        TextBox_SetRect(&list->view.nameBox[i], 0xAF, 0x1FF, 0x6A, 0x139);
        TextBox_SetUnk80(&list->view.nameBox[i], 1);
        TextBox_Init(&list->view.formBox[i], list->formText, 4);
        TextBox_SetRect(&list->view.formBox[i], 0xAF, 0x1FF, 0x6A, 0x139);
        TextBox_SetUnk80(&list->view.formBox[i], 1);
    }
    TextBox_Init(&list->view.nameBoxB, list->nameText, 1);
    TextBox_SetUnk80(&list->view.nameBoxB, 1);
    TextBox_Init(&list->view.formBoxB, list->formText, 3);
    TextBox_SetUnk80(&list->view.formBoxB, 1);
    TextBox_Init(&list->view.nameBoxL, list->nameText, 1);
    TextBox_SetUnk80(&list->view.nameBoxL, 1);
    TextBox_Init(&list->view.formBoxL, list->formText, 3);
    TextBox_SetUnk80(&list->view.formBoxL, 1);
    list->itemText = MPACK_AT(list->res, 21);
    for (j = 0; j < DCLIST_ITEM_ROWS; j++) {
        TextBox_Init(&list->view.itemBox[j], list->itemText, 2);
        TextBox_SetUnk80(&list->view.itemBox[j], 1);
    }
    list->msgText = MPACK_AT(list->res, 13);
    MsgWin_Init(MPACK_AT(list->res, 11), list->msgText, 0, (s32)list->unkC44);
    MsgWin_Open();
    list->voiceLine = -1;
    list->dialogMsg = MPACK_AT(list->res, 22);
    Dialog_Init(MPACK_AT(list->res, 12), list->dialogMsg, 0);
    PassWin_Init(MPACK_AT(list->res, 10));
    ItemHelp_Init(MPACK_AT(list->res, 16));
    list->itemTbl = (ZItemEntry *)MPACK_AT(list->res, 15);
    list->subtitles = MPACK_AT(list->res, 14);
}

/* Draws the screen: the list movie, the message window, the item page, then the windows on top. */
/*
 * NOT MATCHING: 2 of 99 instructions (the attempt is behaviourally exact). In the call
 * DcView_LightRow(view, &list->list, 1) the original loads a2 = 1 before a1 = list + 0xCB0 (the `li` sits in the
 * delay slot of the preceding branch); here the two are the other way round. Nothing else differs.
 */
#if 0
void DcList_Draw(DcList *list) {
    MFlashRef ref;
    DcListView *view;

    if (!(list->flags & DCLIST_STARTED)) {
        Flash_GotoLabel(&list->view.flash[0], "fl_chara_list_in", 1);
        list->flags |= DCLIST_STARTED;
    }
    view = &list->view;
    if (!(list->flags & DCLIST_GREETED)) {
        if (list->view.flash[0].flags & MFLASH_PAD) {
            DcView_LightRow(view, &list->list, 1);
            DcView_LightMenu(view, list->cursor, 1);
            DcList_LightItemRow(view, list->itemCursor, 1);
            DcList_Say(list, 0x1A);
            list->flags |= DCLIST_GREETED;
        }
    }
    DcList_DrawList(list);
    Flash_FindLabel(&view->flash[0], NULL, "mc_single_chara_r", &ref);
    Flash_ClipSetAlpha(&view->flash[0], &ref, list->faceAlpha);
    DcView_HideNamePlates(view);
    DcView_SetMenuText(view);
    DcView_SetStatus(view, &list->status);
    DcView_SetNameText(view, list->status.rec.chara);
    DcList_DrawItemPage(list);
    DcList_UpdateGuide(list);
    DcList_ScrollCloud(list);
    Sprite_DrawPicture(list->view.bg, 0, 0, 0x80);
    Flash_Draw(&view->flash[0]);
    MsgWin_Draw(0, 0, list->voiceLine);
    Flash_Draw(&list->view.flash[1]);
    DcList_DrawDialog(list);
    DcList_DrawPassword(list);
    DcList_DrawItemHelp(list);
}
#else
INCLUDE_ASM("asm/nonmatchings/menu/menu_z", DcList_Draw);
#endif

/* Destroys the movies, frees the buffers and closes the shared windows. */
void DcList_Term(DcList *list) {
    s32 i;

    for (i = 0; i < DCLIST_FLASH_NUM; i++) {
        Flash_Destroy(&list->view.flash[i]);
    }
    if (list->res != NULL) {
        Heap_Free(list->res);
        list->res = NULL;
    }
    if (list->faceRes != NULL) {
        Heap_Free(list->faceRes);
        list->faceRes = NULL;
    }
    if (list->faceFile != NULL) {
        Heap_Free(list->faceFile);
        list->faceFile = NULL;
    }
    PassWin_Term();
    Dialog_Term();
    MsgWin_Term();
    ItemHelp_Term();
}

/* Reads the pad for the current state once the list movie accepts input. */
void DcList_Input(DcList *list) {
    if (!(list->view.flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (list->flags & DCLIST_LEAVE) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    switch (list->state) {
    case DCLIST_ST_LIST:
        DcList_InputList(list);
        break;
    case DCLIST_ST_MENU:
        DcList_InputMenu(list);
        break;
    case DCLIST_ST_ITEMS:
        DcList_InputItems(list);
        break;
    case DCLIST_ST_ITEM_HELP:
        DcList_InputItemHelp(list);
        break;
    case DCLIST_ST_PASSWORD:
        DcList_InputPassword(list);
        break;
    case DCLIST_ST_DIALOG:
        DcList_InputDialog(list);
        break;
    case DCLIST_ST_DELETED:
        DcList_InputDeleted(list);
        break;
    }
}

/* Once the screen is to be left: starts the fade out and returns 1 when it is over. */
s32 DcList_CheckLeave(DcList *list) {
    if (list->flags & DCLIST_LEAVE) {
        if (!(list->flags & DCLIST_LEAVING)) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            list->flags |= DCLIST_LEAVING;
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
        } else {
            return 1;
        }
    }
    return 0;
}

/* Runs the saved-character list (progress mode 55) with its own frame loop; the work is on the stack. */
s32 DcList_Run(s32 section) {
    DcList list;

    memset(&list, 0, sizeof(DcList));
    DcList_Reset(&list);
    list.section = section;
    DcList_Init(&list);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        DcList_Update(&list);
        DcList_Draw(&list);
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (DcList_CheckLeave(&list)) {
            break;
        }
        DcList_Input(&list);
        DcList_UpdateFace(&list);
    }
    DcList_Term(&list);
    Dma_ResetBuffers();
    return list.result;
}
