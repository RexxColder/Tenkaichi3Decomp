#include "common.h"
#include "menu/menu_y.h"

/*
 * Menu overlay DBZP.BIN, 0x3A65E8..0x3A7D98: head of the DcList object, the custom character list of the
 * Data Center (modes 53..56, handler 0x3A9850; the object continues in the next chunk, menu_z, whose
 * functions 0x3A93B8 (draw), 0x3A9600 (input) and 0x3A9768 (init) call these).
 *
 * Object boundary: the first function follows Option_Term, the last function of the option screen object;
 * the object's read-only data starts at 0x3BCA00 ("mc_guide_blma_eye"), behind a block of initialised data
 * (0x3BC928..0x3BC9F8: the password keyboard tables of this link group).
 *
 * The code is written against small sub-structures of the work (DcView, DcChars, DcStatus), each passed by
 * pointer; DcChars_GetRec returns a record by value.
 */

extern char *strcpy(char *, const char *);

/*
 * Small helpers that the original inlined: each use has its own clip reference on the stack (16 bytes apart)
 * whose address is worked out again at every use, which is what an inlined function's locals look like.
 */

/* Gives a child clip its texture rectangle. */
static inline void DcView_SetChildUv(DcView *v, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(v->flash, parent, name, &ref);
    Flash_ClipSetUv(v->flash, &ref, uv);
}

/* Shows or hides a child clip. */
static inline void DcView_ShowChild(DcView *v, char *parent, char *name, s32 on) {
    MFlashRef ref;

    Flash_FindLabel(v->flash, parent, name, &ref);
    Flash_ClipSetFlags(v->flash, &ref, 2, on);
}

/* Texture rectangle of a row of a sheet of w x h pictures. */
static inline void DcUv_SetRow(MFlashUv *uv, s32 row, s32 w, s32 h) {
    uv->x0 = 0;
    uv->y0 = row * h;
    uv->x1 = w;
    uv->y1 = row * h + h;
}

/* Starts the guide's blink timer at a random phase. */
void DcView_InitBlink(DcView *v) {
    v->blink = Rand_Range(0x20);
}

/* Copies the 14 saved custom characters out of the save; no row is scrolling. */
void DcChars_Load(DcChars *c) {
    s32 i;

    c->extra = -1;
    for (i = 0; i < DC_REC_NUM; i++) {
        /* gSaveData->rec[i]; the byte offset is worked out first in the original */
        s32 off = i * sizeof(DcRec) + 0x2D40;

        c->rec[i] = *(DcRec *)((u8 *)gSaveData + off);
    }
}

/* Works out what the record's items add up to. */
void DcStatus_Calc(DcStatus *s, DcItemTbl *table) {
    ItemSet_GetBonus(s->rec.item, table, s->bonus);
}

/* Resets the list's state for a new visit. */
void DcList_Reset(DcList *d) {
    DcView_InitBlink(&d->view);
    DcChars_Load(&d->chars);
    d->unkE7C = 0x1E;
}

/* Animates the guide's (Bulma's) eyes and mouth. */
void DcList_UpdateGuide(DcList *d) {
    MFlashRef ref;
    MFlash *flash = d->view.flash;

    Flash_FindLabel(flash, NULL, "mc_guide_blma_eye", &ref);
    FlashAnim_Blink(flash, &ref, &d->view.blink, 0);
    Flash_FindLabel(flash, NULL, "mc_guide_blma_mouth", &ref);
    if (Voice_GetStat(0) != Y_VOICE_IDLE) {
        FlashAnim_Talk(flash, &ref, &d->view.talk, 0);
    } else {
        FlashAnim_ShowNext2(flash, &ref, 0);
    }
}

/* Scrolls the backdrop strip "mc_compane_3". */
void DcList_ScrollCloud(DcList *d) {
    MFlashRef ref;
    MFlashUv uv;
    MFlash *flash = d->view.flash;

    uv.y0 = 0;
    uv.y1 = 0x40;
    uv.x0 = 0;
    uv.x1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_compane_3", &ref);
    FlashAnim_Scroll(flash, &ref, &uv, &d->cloudX, NULL, -0.15238095f, 0.0f);
}

/* Clip callback: limits drawing to the list area. */
void DcList_SetListScissor(void) {
    Sprite_SetScissor(0xAF, 0x1FF, 0x6A, 0x139);
}

/* Clip callback: back to the whole screen. */
void DcList_ResetScissor(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* Hangs the scissor callbacks on the four list plates. */
void DcView_SetRowCallbacks(DcView *v) {
    MFlashRef ref;
    char name[0x100];
    s32 i;

    for (i = 0; i < 4; i++) {
        sprintf(name, "mc_menu_plate2_%d", i + 1);
        Flash_FindLabel(v->flash, NULL, name, &ref);
        Flash_ClipSetCallbackA(v->flash, &ref, DcList_SetListScissor, NULL);
        Flash_ClipSetCallbackB(v->flash, &ref, DcList_ResetScissor, NULL);
    }
}

/* Hides the up or the down arrow of the list. */
void DcView_HideArrow(DcView *v, s32 up) {
    MFlashRef ref;
    char parent[0x100];
    char name[0x100];

    if (up) {
        strcpy(parent, "mc_yajirusi_up");
        strcpy(name, "mc_yajirusi_icon_up");
    } else {
        strcpy(parent, "mc_yajirusi_down");
        strcpy(name, "mc_yajirusi_icon_down");
    }
    Flash_FindLabel(v->flash, parent, name, &ref);
    Flash_ClipSetFlags(v->flash, &ref, 2, 0);
}

/* Gives the two arrows their pictures and hides the one that cannot be used. */
void DcView_SetArrows(DcView *v, DcChars *c) {
    MFlashUv uv;
    s32 top;

    uv.x0 = 0;
    uv.x1 = 0x20;
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    DcView_SetChildUv(v, "mc_yajirusi_up", "mc_yajirusi_icon_up", &uv);
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    DcView_SetChildUv(v, "mc_yajirusi_down", "mc_yajirusi_icon_down", &uv);
    top = c->top;
    if (top == 0) {
        DcView_HideArrow(v, 1);
    } else if (top == DC_TOP_MAX) {
        DcView_HideArrow(v, 0);
    }
}

/* Places the scroll bar's knob. */
void DcView_SetScrollBar(DcView *v, DcChars *c) {
    MFlashRef ref;
    s32 n = DC_REC_NUM;
    s32 y = c->top * 0xB5 / n;

    Flash_FindLabel(v->flash, NULL, "mc_scroll_bar_point", &ref);
    Flash_ClipSetScale(v->flash, &ref, 1.0f, 1.2128571f);
    Flash_ClipSetOffset(v->flash, &ref, 0, y);
}

/* Lights or dims the plate the cursor is on. */
void DcView_LightRow(DcView *v, DcChars *c, s32 on) {
    MFlashRef ref;
    char name[0x100];

    sprintf(name, "mc_menu_plate2_%d", c->row + 1);
    Flash_FindLabel(v->flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(v->flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(v->flash, &ref, "fl_off_start");
    }
}

/* Keeps the list's top inside 0..11; 0 when it had to be corrected. */
s32 DcChars_ClampTop(DcChars *c) {
    if (c->top < 0) {
        c->top = 0;
        return 0;
    }
    if (c->top > DC_TOP_MAX) {
        c->top = DC_TOP_MAX;
        return 0;
    }
    return 1;
}

/* Keeps the cursor inside the three plates; 1 when it had to be corrected (the list must scroll). */
s32 DcChars_ClampRow(DcChars *c) {
    if (c->row < 0) {
        c->row = 0;
        return 1;
    }
    if (c->row > DC_ROWS - 1) {
        c->row = DC_ROWS - 1;
        return 1;
    }
    return 0;
}

/* Fills the four list plates: name and form of each record, empty records dimmed. */
void DcView_SetRows(DcView *v, DcChars *c) {
    MFlashRef ref;
    char name[0x100];
    s32 i;
    s32 chara;

    for (i = 0; i < 4; i++) {
        sprintf(name, "mc_menu_plate2_%d", i + 1);
        if (i != 3) {
            chara = c->rec[i + c->top].chara;
        } else {
            chara = c->rec[c->extra].chara;
        }
        if (i == c->row) {
            if (chara == -1) {
                Flash_FindLabel(v->flash, NULL, name, &ref);
                Flash_ClipSetAlpha(v->flash, &ref, 1.0f);
                continue;
            }
        } else if (chara == -1) {
            Flash_FindLabel(v->flash, NULL, name, &ref);
            Flash_ClipSetAlpha(v->flash, &ref, 0.5f);
            continue;
        }
        Flash_FindLabel(v->flash, name, "mc_menu_text1_on", &ref);
        TextBox_AttachLine(v->flash, &ref, 0, 0, chara, &v->name[i]);
        Flash_FindLabel(v->flash, name, "mc_menu_text2_on", &ref);
        TextBox_AttachLine(v->flash, &ref, 0, 0, chara, &v->form[i]);
        Flash_FindLabel(v->flash, NULL, name, &ref);
        Flash_ClipSetAlpha(v->flash, &ref, 1.0f);
    }
}

/* The record the cursor is on. */
s32 DcChars_GetCursor(DcChars *c) {
    return c->top + c->row;
}

/* A copy of the record the cursor is on. */
DcRec DcChars_GetRec(DcChars *c) {
    return c->rec[DcChars_GetCursor(c)];
}

/* Takes the record under the cursor as the one the details panel shows. */
void DcList_PickRec(DcList *d) {
    DcStatus *s = &d->status;
    DcChrEntry *tbl;

    d->status.rec = DcChars_GetRec(&d->chars);
    DcStatus_Calc(s, d->itemTbl);
    tbl = (DcChrEntry *)MPACK_AT(gCommonRes->chrFile, 1);
    s->attr = (tbl[s->rec.chara].flags ^ 1) & 1;
}

/* Plays the "chosen" animation of the cursor's plate. */
void DcList_RowOk(DcList *d) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash;

    sprintf(name, "mc_menu_plate2_%d", d->chars.row + 1);
    flash = d->view.flash;
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_ok");
}

/* Per frame: sets up everything the list part of the movie shows. */
void DcList_DrawList(DcList *d) {
    DcView *v = &d->view;
    DcChars *c = &d->chars;

    DcView_SetRowCallbacks(v);
    DcView_SetArrows(v, c);
    DcView_SetScrollBar(v, c);
    DcView_SetRows(v, c);
}

/* Input of the list: up / down one record, left / right three, confirm opens the details, cancel leaves. */
void DcList_InputList(DcList *d) {
    DcRec rec;

    if (gPad[0].gameRepeat & 8) {
        DcChars *c = &d->chars;

        if (DcChars_GetCursor(c) != 0) {
            DcView *v = &d->view;

            DcView_LightRow(v, c, 0);
            d->chars.row--;
            if (DcChars_ClampRow(c)) {
                c->top--;
                if (DcChars_ClampTop(c)) {
                    Flash_GotoLabel(v->flash, "fl_chara_list_down", 1);
                    d->chars.extra = c->top + 3;
                }
            }
            DcView_LightRow(v, c, 1);
            Snd_PlaySe(1, 0);
            return;
        }
    }
    if (gPad[0].gameRepeat & 4) {
        DcChars *c = &d->chars;

        if (DcChars_GetCursor(c) != DC_REC_NUM - 1) {
            DcView *v = &d->view;

            DcView_LightRow(v, c, 0);
            d->chars.row++;
            if (DcChars_ClampRow(c)) {
                c->top++;
                if (DcChars_ClampTop(c)) {
                    Flash_GotoLabel(v->flash, "fl_chara_list_up", 1);
                    d->chars.extra = c->top - 1;
                }
            }
            DcView_LightRow(v, c, 1);
            Snd_PlaySe(1, 0);
            return;
        }
    }
    if (gPad[0].gamePressed & 0x200) {
        rec = DcChars_GetRec(&d->chars);
        if (rec.chara >= 0) {
            DcView *v;

            DcList_RowOk(d);
            v = &d->view;
            Flash_GotoLabel(v->flash, "fl_chara_details_in", 1);
            MsgWin_Close();
            d->state = DCLIST_ST_MENU;
            DcView_LightMenu(v, d->menuCursor, 0);
            d->menuCursor = 0;
            DcView_LightMenu(v, 0, 1);
            DcList_PickRec(d);
            d->flags |= DCLIST_OPENED;
            Snd_PlaySe(1, 1);
        } else {
            Snd_PlaySe(1, 7);
        }
        DcList_RowOk(d);
        return;
    }
    if (gPad[0].gamePressed & 0x400) {
        d->running = 0;
        d->flags |= DCLIST_LEAVING;
        Flash_GotoLabel(d->view.flash, "fl_chara_list_cancel", 1);
        MsgWin_Close();
        Snd_PlaySe(1, 2);
        return;
    }
    if (gPad[0].gamePressed & 1) {
        s32 top = d->chars.top;

        if (top + 2 >= 3) {
            DcChars *c;

            d->chars.top = top - 3;
            c = &d->chars;
            DcChars_ClampTop(c);
            Flash_GotoLabel(d->view.flash, "fl_chara_list_down", 1);
            d->chars.extra = c->top + 3;
            Snd_PlaySe(1, 0);
            return;
        }
    }
    if (gPad[0].gamePressed & 2) {
        s32 top = d->chars.top;

        if (top != DC_TOP_MAX) {
            DcChars *c;

            d->chars.top = top + 3;
            c = &d->chars;
            DcChars_ClampTop(c);
            Flash_GotoLabel(d->view.flash, "fl_chara_list_up", 1);
            d->chars.extra = c->top - 1;
            Snd_PlaySe(1, 0);
        }
    }
}

/* Gives the three plates of the details menu their text pictures. */
void DcView_SetMenuText(DcView *v) {
    MFlashUv uv;
    char name[0x100];
    s32 i;

    for (i = 0; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x100;
        uv.y1 = i * 0x20 + 0x20;
        sprintf(name, "menu_plate_%d", i + 1);
        DcView_SetChildUv(v, name, "mc_menu_text_on", &uv);
        DcView_SetChildUv(v, name, "mc_menu_text_off", &uv);
    }
}

/* Draws the status panel: used item slots, the four stat changes as rows of marks, the attribute. */
void DcView_SetStatus(DcView *v, DcStatus *s) {
    MFlashUv uv;
    char slots[0x100];
    char base[0x100];
    char name[0x100];
    s32 i;
    s32 k;
    s32 val;

    sprintf(slots, "mc_status_ability_base%d_%d", 2, 1);
    for (i = s->bonus[0]; i < 7; i++) {
        sprintf(name, "mc_status_ability_plus_%d", i + 4);
        DcView_ShowChild(v, slots, name, 0);
    }
    DcUv_SetRow(&uv, i, 0x80, 0x14);
    sprintf(name, "mc_text_ability_%d", 1);
    DcView_SetChildUv(v, slots, name, &uv);
    for (i = 0; i < 4; i++) {
        val = s->bonus[1 + i];
        sprintf(base, "mc_status_ability_base%d_%d", 1, i + 1);
        for (k = 0; k < 4; k++) {
            sprintf(name, "mc_status_ability_minus_%d", k + 1);
            DcView_ShowChild(v, base, name, 0);
            sprintf(name, "mc_status_ability_plus_%d", k + 1);
            DcView_ShowChild(v, base, name, 0);
            if (val < 0 && val < -k && -k <= 0) {
                sprintf(name, "mc_status_ability_minus_%d", k + 1);
                DcView_ShowChild(v, base, name, 1);
            } else if (val > 0 && k >= 0 && k < val) {
                sprintf(name, "mc_status_ability_plus_%d", k + 1);
                DcView_ShowChild(v, base, name, 1);
            }
        }
        DcUv_SetRow(&uv, i + 1, 0x80, 0x14);
        sprintf(name, "mc_text_ability_%d", 1);
        DcView_SetChildUv(v, base, name, &uv);
    }
    {
        MFlashRef ref;

        Flash_FindLabel(v->flash, NULL, "mc_status_attribute", &ref);
        Flash_ClipSetTex(v->flash, &ref, s->attr);
    }
}

/* Lights or dims a plate of the details menu. */
void DcView_LightMenu(DcView *v, s32 item, s32 on) {
    MFlashRef ref;
    char name[0x100];

    sprintf(name, "menu_plate_%d", item + 1);
    Flash_FindLabel(v->flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(v->flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(v->flash, &ref, "fl_off_start");
    }
}

/* Wraps the details menu's cursor into 0..2. */
void DcList_WrapMenu(s32 *cursor) {
    s32 max = 2;

    if (*cursor < 0) {
        *cursor = max;
    } else if (*cursor > max) {
        *cursor = 0;
    }
}

/* Hides the backing plates of the name and form text. */
void DcView_HideNamePlates(DcView *v) {
    MFlashRef ref;

    Flash_FindLabel(v->flash, "mc_name_plate_l", "ch_name_plate", &ref);
    Flash_ClipSetFlags(v->flash, &ref, 2, 0);
    Flash_FindLabel(v->flash, "mc_form_plate_l", "ch_form_plate", &ref);
    Flash_ClipSetFlags(v->flash, &ref, 2, 0);
}

/* Shows a character's name and form in the details panel. */
void DcView_SetNameText(DcView *v, s32 line) {
    MFlashRef ref;

    Flash_FindLabel(v->flash, NULL, "mc_name_text_l", &ref);
    TextBox_AttachLine(v->flash, &ref, 0, 0, line, &v->curName);
    Flash_FindLabel(v->flash, NULL, "mc_form_text_l", &ref);
    TextBox_AttachLine(v->flash, &ref, 0, 0, line, &v->curForm);
}

/*
 * Sets how opaque the character picture is. Inlined into the draw function of the next chunk (0x3A9478); its
 * string lies here in the object's read-only data, between those of DcView_SetNameText and DcView_LightItem,
 * so this is where it was defined.
 */
static inline void DcView_SetPictureAlpha(DcView *v, f32 alpha) {
    MFlashRef ref;

    Flash_FindLabel(v->flash, NULL, "mc_single_chara_r", &ref);
    Flash_ClipSetAlpha(v->flash, &ref, alpha);
}

/* Plays the "chosen" animation of a plate of the details menu. */
void DcView_MenuOk(DcView *v, s32 item) {
    MFlashRef ref;
    char name[0x100];

    sprintf(name, "menu_plate_%d", item + 1);
    Flash_FindLabel(v->flash, NULL, name, &ref);
    Flash_ClipGotoLabel(v->flash, &ref, "fl_ok");
}

/* Whether an item slot holds no valid item id; never true for the last slot. */
s32 DcRec_IsSlotEmpty(u16 *items, s32 slot) {
    s32 empty = 0;

    items += slot;
    if ((u16)(*items - 1) >= DC_ITEM_MAX) {
        empty = slot != 7;
    }
    return empty;
}

/* Lights or dims a row of the item list (the second movie). */
void DcView_LightItem(DcView *v, s32 row, s32 on) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash;

    sprintf(name, "mc_list_plate_%d", row + 1);
    flash = &v->flash[1];
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Input of the details menu: 0 opens the item list, 1 builds the password, 2 asks whether to delete. */
void DcList_InputMenu(DcList *d) {
    DcRec rec;

    if (gPad[0].gamePressed & 0x200) {
        DcView *v = &d->view;

        DcView_MenuOk(v, d->menuCursor);
        switch (d->menuCursor) {
            case 0:
                d->state = DCLIST_ST_ITEMS;
                Flash_GotoLabel(&d->view.flash[1], "fl_evo_in", 1);
                DcView_LightItem(v, d->itemCursor, 0);
                d->itemCursor = 0;
                while (DcRec_IsSlotEmpty(d->status.rec.item, d->itemCursor)) {
                    d->itemCursor++;
                }
                DcView_LightItem(v, d->itemCursor, 1);
                Snd_PlaySe(1, 1);
                break;
            case 1:
                rec = DcChars_GetRec(&d->chars);
                func_003AEA28(&rec, rec.chara, rec.level);
                d->state = DCLIST_ST_PASSWORD;
                Snd_PlaySe(1, 1);
                break;
            case 2:
                d->state = DCLIST_ST_DELETE;
                Dialog_SetMsg(2);
                Dialog_Start(0);
                Dialog_SetCursor(1);
                Dialog_SetChoices(1);
                Snd_PlaySe(1, 1);
                break;
        }
    } else if (gPad[0].gamePressed & 0x400) {
        Flash_GotoLabel(d->view.flash, "fl_chara_details_out", 1);
        MsgWin_Open();
        d->state = DCLIST_ST_LIST;
        Snd_PlaySe(1, 2);
    } else if (gPad[0].gameRepeat & 8) {
        DcView *v = &d->view;
        s32 *cursor;

        DcView_LightMenu(v, d->menuCursor, 0);
        d->menuCursor--;
        cursor = &d->menuCursor;
        DcList_WrapMenu(cursor);
        DcView_LightMenu(v, *cursor, 1);
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gameRepeat & 4) {
        DcView *v = &d->view;
        s32 *cursor;

        DcView_LightMenu(v, d->menuCursor, 0);
        d->menuCursor++;
        cursor = &d->menuCursor;
        DcList_WrapMenu(cursor);
        DcView_LightMenu(v, *cursor, 1);
        Snd_PlaySe(1, 0);
    }
}
