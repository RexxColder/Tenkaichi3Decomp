#include "common.h"
#include "menu/menu_z.h"
#include "sys/misc_a.h"

/*
 * Menu overlay DBZP.BIN, 0x3AAF30..0x3AC440: head of the DcPass object, the password screen of the Data Center
 * (progress mode 54): an on-screen keyboard of two pages, 34 cells of text, and the status page of the
 * character a password decodes to. The object's read-only data starts at 0x3BD700 ("mc_input_code_%d_%02d");
 * its .data is the key table at 0x3BC928. Init / Input / Run are in the next chunk.
 */

/* The on-screen keyboard: [page][row][col]; codes 0..6 are DCKEY_. */
extern char gDcPassKeys[DCPASS_PAGES][DCPASS_ROWS][DCPASS_COLS]; /* 0x3BC928 */

/* Keeps a value in lo..hi as a ring. */
static inline s32 DcPass_Wrap(s32 value, s32 lo, s32 hi) {
    if (value < lo) {
        return hi;
    }
    if (value > hi) {
        return lo;
    }
    return value;
}

/* Sets the texture rectangle of a clip found by name (each use has its own MFlashRef on the stack). */
static inline void DcPass_SetUv(MFlash *flash, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Texture rectangle of one cell of a sheet. */
static inline void DcPass_SetCell(MFlashUv *uv, s32 col, s32 row, s32 w, s32 h) {
    uv->x0 = col * w;
    uv->y0 = row * h;
    uv->x1 = uv->x0 + w;
    uv->y1 = uv->y0 + h;
}

/* Keeps a three-way cursor in 0..2. */
s32 DcPass_Wrap3(s32 value) {
    return DcPass_Wrap(value, 0, 2);
}

/* The character (or DCKEY_ code) of a keyboard cell; the wide "0" key gives '0'. */
char DcPass_GetKey(DcKeyPos *pos) {
    char key = gDcPassKeys[pos->page][pos->row][pos->col];

    if (key == DCKEY_ZERO) {
        key = '0';
    }
    return key;
}

/* Keeps a keyboard position inside the keyboard (every axis is a ring). */
/*
 * NOT MATCHING: the three wraps compile to the same conditional-move sequence here; the original has that
 * sequence for the row only and another (the one DcPass_Wrap3 has as a function of its own) for page and
 * column. Same results for every input.
 */
#if 0
void DcPass_WrapPos(DcKeyPos *pos) {
    pos->page = DcPass_Wrap(pos->page, 0, DCPASS_PAGES - 1);
    pos->row = DcPass_Wrap(pos->row, 0, DCPASS_ROWS - 1);
    pos->col = DcPass_Wrap(pos->col, 0, DCPASS_COLS - 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/menu/menu_z_d", DcPass_WrapPos);
#endif

/* Puts a keyboard position on the middle (1) or right (2) button of the bottom row. */
void DcPass_GotoButton(s32 button, DcKeyPos *pos) {
    switch (button) {
    case 2:
        pos->row = DCPASS_ROWS - 1;
        pos->col = 12;
        break;
    case 1:
        pos->row = DCPASS_ROWS - 1;
        pos->col = 6;
        break;
    }
}

/* Whether the cell in another column of the same row belongs to the same (wide) key. */
s32 DcPass_IsSameKeyCol(DcKeyPos *pos, s32 col) {
    DcKeyPos other = *pos;

    other.col = col;
    return DcPass_GetKey(pos) == DcPass_GetKey(&other);
}

/* Whether the cell in another row of the same column belongs to the same key. */
s32 DcPass_IsSameKeyRow(DcKeyPos *pos, s32 row) {
    DcKeyPos other = *pos;

    other.row = row;
    return DcPass_GetKey(pos) == DcPass_GetKey(&other);
}

/* Turns a keyboard position into the row / column numbers of its clip ("mc_input_code_<row>_<col>"). */
void DcPass_GetClipPos(DcKeyPos *pos, DcKeyPos *out) {
    *out = *pos;
    if (pos->row == 1) {
        if (pos->col >= 12) {
            out->row++;
        }
    } else if (pos->row == 3) {
        out->col -= 2;
        if (out->col < 0) {
            out->col = 0;
        }
        if (pos->col >= 12) {
            out->col = 10;
        }
    } else if (pos->row == 4) {
        out->col = DcPass_GetKey(pos);
    }
}

/* The keyboard cell that types a character (digits are on row 3 of the first page). */
DcKeyPos DcPass_FindKey(char c) {
    DcKeyPos pos;
    char digit[2];

    if (c >= '0' && c <= '9') {
        digit[0] = c;
        digit[1] = 0;
        pos.page = 0;
        pos.row = 3;
        pos.col = atoi(digit);
        return pos;
    }
    for (pos.page = 0; pos.page < DCPASS_PAGES; pos.page++) {
        for (pos.row = 0; pos.row < DCPASS_ROWS; pos.row++) {
            for (pos.col = 0; pos.col < DCPASS_COLS; pos.col++) {
                if (c == DcPass_GetKey(&pos)) {
                    return pos;
                }
            }
        }
    }
    return pos;
}

/*
 * Cuts the caps of the four rows of keys from the sheet. The kind of key (small letter, wide key) is taken from
 * the key at *cur, not from the cell being drawn.
 */
void DcPass_DrawKeys(MFlash *flash, DcKeyPos *cur) {
    DcKeyPos out;
    DcKeyPos pos;
    MFlashUv uv;
    char name[0x100];
    char key;
    s32 shift;

    pos.page = cur->page;
    for (pos.row = 0; pos.row < DCPASS_ROWS - 1; pos.row++) {
        for (pos.col = 0; pos.col < DCPASS_COLS; pos.col++) {
            key = DcPass_GetKey(cur);
            DcPass_GetClipPos(&pos, &out);
            shift = (key >= 'a' && key <= 'z') ? 4 : 0;
            DcPass_SetCell(&uv, out.col, out.row + shift, 0x20, 0x28);
            sprintf(name, "mc_input_code_%d_%02d", out.row + 1, out.col + 1);
            /* no braces: each use keeps its own MFlashRef */
            if (key == DCKEY_WIDE_D1 || key == DCKEY_WIDE_D2)
                DcPass_SetUv(flash, name, "mc_text_input_code_d_on", &uv);
            else if (key == DCKEY_WIDE_E)
                DcPass_SetUv(flash, name, "mc_text_input_code_e_on", &uv);
            else
                DcPass_SetUv(flash, name, "mc_text_input_code_a_on", &uv);
        }
    }
}

/* Cuts the captions of the three bottom buttons and the keyboard's lettering for the page shown. */
void DcPass_DrawButtons(MFlash *flash, s32 page) {
    char name[0x100];
    MFlashUv uv;

    DcPass_SetCell(&uv, 0, 1, 0x100, 0x40);
    sprintf(name, "mc_input_code_5_%02d", 1);
    DcPass_SetUv(flash, name, "mc_text_input_code_b_on", &uv);
    DcPass_SetUv(flash, name, "mc_text_input_code_b_off", &uv);
    if (page == 0) {
        DcPass_SetCell(&uv, 0, 3, 0x100, 0x40);
    } else {
        uv.x0 = 0;
        uv.x1 = 0x100;
        uv.y0 = 0x80;
        uv.y1 = 0xC0;
    }
    sprintf(name, "mc_input_code_5_%02d", 2);
    DcPass_SetUv(flash, name, "mc_text_input_code_c_on", &uv);
    DcPass_SetUv(flash, name, "mc_text_input_code_c_off", &uv);
    DcPass_SetCell(&uv, 0, 0, 0x100, 0x40);
    sprintf(name, "mc_input_code_5_%02d", 3);
    DcPass_SetUv(flash, name, "mc_text_input_code_b_on", &uv);
    DcPass_SetUv(flash, name, "mc_text_input_code_b_off", &uv);
    uv.x0 = 0;
    uv.x1 = 0x200;
    uv.y0 = page << 7;
    uv.y1 = (page << 7) + 0x80;
    DcPass_SetUv(flash, NULL, "mc_text_keyboard", &uv);
}

/* Wraps a keyboard position and lights or dims its key. */
void DcPass_LightKey(MFlash *flash, DcKeyPos *pos, s32 on) {
    MFlashRef ref;
    DcKeyPos out;
    char name[0x100];

    DcPass_WrapPos(pos);
    DcPass_GetClipPos(pos, &out);
    sprintf(name, "mc_input_code_%d_%02d", out.row + 1, out.col + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Plays the "pressed" animation of a key. */
void DcPass_FlashKey(MFlash *flash, DcKeyPos *pos) {
    MFlashRef ref;
    DcKeyPos out;
    char name[0x100];

    DcPass_GetClipPos(pos, &out);
    sprintf(name, "mc_input_code_%d_%02d", out.row + 1, out.col + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_ok");
}

/* Draws the keyboard of the page shown. */
void DcPass_DrawKeyboard(DcPass *pass) {
    MFlash *flash = &pass->view.flash[0];
    DcKeyPos *pos = &pass->keyPos;

    DcPass_DrawKeys(flash, pos);
    DcPass_DrawButtons(flash, pos->page);
}

/* Empties the text: 34 spaces. */
void DcPassText_Clear(DcPassText *text) {
    s32 i;

    for (i = 0; i < DCPASS_TEXT_LEN; i++) {
        text->text[i] = ' ';
    }
    text->text[DCPASS_TEXT_LEN] = 0;
}

/* Types the character of a key at the text cursor and moves the cursor on (it stops on the last cell). */
void DcPassText_Put(DcKeyPos *pos, DcPassText *text) {
    text->text[text->cursor] = DcPass_GetKey(pos);
    if (text->cursor < DCPASS_TEXT_LEN - 1) {
        text->cursor++;
    }
}

/* Backspace: clears the cell under the cursor, or steps back when it is empty. Returns 1 if nothing was left. */
s32 DcPassText_Delete(DcPassText *text) {
    if (text->cursor == 0) {
        if (text->text[0] == ' ') {
            return 1;
        }
    } else if (text->text[text->cursor] == ' ') {
        text->cursor--;
        text->text[text->cursor] = ' ';
        goto done;
    }
    text->text[text->cursor--] = ' ';
done:
    if (text->cursor < 0) {
        text->cursor = 0;
    }
    return 0;
}

/* Whether all 34 cells are filled. */
s32 DcPassText_IsFull(DcPassText *text) {
    s32 i;

    for (i = 0; i < DCPASS_TEXT_LEN; i++) {
        if (text->text[i] == ' ') {
            return 0;
        }
    }
    return 1;
}

/* Shows the text: every cell gets the cap of the key that types its character. */
void DcPass_DrawText(MFlash *flash, char *text) {
    MFlashUv uv;
    char name[0x100];
    DcKeyPos pos;
    s32 i;

    for (i = 0; i < DCPASS_TEXT_LEN + 1; i++) {
        char c = text[i];

        pos = DcPass_FindKey(c);
        if (c >= 'a' && c <= 'z') {
            DcPass_SetCell(&uv, pos.col, pos.row + 4, 0x20, 0x28);
        } else {
            DcPass_SetCell(&uv, pos.col, pos.row, 0x20, 0x28);
        }
        if (i < 17) {
            sprintf(name, "mc_input_pass_%d_%02d", 1, i + 1);
        } else {
            sprintf(name, "mc_input_pass_%d_%02d", 2, i - 16);
        }
        DcPass_SetUv(flash, NULL, name, &uv);
    }
}

/* Puts the cursor clip under the cell the next character goes to. */
void DcPass_DrawCursor(MFlash *flash, DcPassText *text) {
    char name[0x100];
    MFlashRef ref;
    s32 x;
    s32 y;
    s32 cx;
    s32 cy;

    if (text->cursor < 17) {
        sprintf(name, "mc_input_pass_%d_%02d", 1, text->cursor + 1);
    } else {
        sprintf(name, "mc_input_pass_%d_%02d", 2, text->cursor - 16);
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGetPos(flash, &ref, &x, &y);
    Flash_FindLabel(flash, NULL, "mc_input_cursor", &ref);
    Flash_ClipSetOffset(flash, &ref, 0, 0);
    Flash_ClipGetPos(flash, &ref, &cx, &cy);
    x -= cx;
    y -= cy;
    Flash_FindLabel(flash, NULL, "mc_input_cursor", &ref);
    Flash_ClipSetOffset(flash, &ref, x, y + 10);
}

/*
 * Copies the text up to its first empty cell into pass and measures it. Returns 1 (not a password) when an
 * empty cell comes before the 33rd, or when only one of the last two cells is filled.
 */
s32 DcPassText_Pack(DcPassText *text) {
    s32 i;

    for (i = 0; text->text[i] != 0; i++) {
        text->pass[i] = text->text[i];
        if (text->text[i] == ' ' && i < 0x20) {
            return 1;
        }
    }
    text->pass[i] = 0;
    if (text->pass[0x20] == ' ') {
        if (text->pass[0x21] == ' ') {
            text->pass[0x20] = 0;
        } else {
            return 1;
        }
    }
    text->len = strlen(text->pass);
    return 0;
}

/* Reads the character of every saved custom character from the save. */
void DcPassList_Refresh(DcPassList *list) {
    s32 i;

    for (i = 0; i < SAVE_REC_COUNT; i++) {
        list->chara[i] = ZSAVE->rec[i].chara;
    }
}

/* Stores the decoded character in the save slot under the list's cursor and marks the save as changed. */
void DcPass_StoreRec(ZStatus *status, DcPassList *list) {
    s32 idx = list->top + list->cur;

    list->chara[idx] = status->chara;
    ZSAVE->rec[idx] = status->rec;
    ZPROG->flags |= ZPROG_DIRTY;
}

/* Whether a character is in the grid of unlocked characters. */
static inline s32 DcPass_IsListed(DcPass *pass, s32 chara) {
    s32 i;

    for (i = 0; i < pass->gridCount; i++) {
        if (pass->grid[i].id == chara) {
            return 1;
        }
    }
    return 0;
}

/*
 * Decodes the typed text: 32 characters are the previous game's format (converted), 34 this game's. On success
 * (the character must be unlocked) fills the status page's record and returns 1.
 */
/*
 * NOT MATCHING: 20 of 118 instructions, block layout only: the original returns after a failed
 * DcPassText_Pack with an unconditional branch (the attempt folds it into the conditional one), and puts the
 * "found" exit of the grid search behind the 34-character validation instead of in front of it; two stores are
 * also scheduled the other way round. Calls, arguments and stores are the same.
 */
#if 0
s32 DcPass_Decode(DcPass *pass) {
    ChrPassData data;
    OldPassChar old;
    ZChrEntry *chrTbl;
    u16 attr;
    s32 i;

    memset(&data, 0, sizeof(ChrPassData));
    if (DcPassText_Pack(&pass->text) == 0) {
        if (pass->text.len == 32) {
            memset(&old, 0, sizeof(OldPassChar));
            if (OldPass_DecodeChar(&old, pass->text.pass) != 1) {
                return 0;
            }
            if (!PassChk_IsOldValid(&old)) {
                return 0;
            }
            PassChk_ConvertOld(&data, &old);
        } else if (pass->text.len == 34) {
            if (ChrPass_Decode(&data, pass->text.pass) != 1) {
                return 0;
            }
            if (!PassChk_IsValid(&data)) {
                return 0;
            }
        } else {
            return 0;
        }
        if (!DcPass_IsListed(pass, data.charId)) {
            return 0;
        }
        pass->status.chara = data.charId;
        pass->status.rec.chara = data.charId;
        for (i = 0; i < 8; i++) {
            pass->status.rec.item[i] = data.item[i];
        }
        pass->status.rec.level = data.extraSlots;
        ItemSet_GetBonus(pass->status.rec.item, pass->itemTbl, &pass->status.val[0]);
        chrTbl = (ZChrEntry *)MPACK_AT(gCommonRes->data[2], 1);
        attr = chrTbl[pass->status.chara].flags;
        pass->flags |= DCPASS_FACE_CHANGE;
        pass->status.attr = (attr ^ 1) & 1;
        DcPassList_Refresh(&pass->list);
        return 1;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/menu/menu_z_d", DcPass_Decode);
#endif

#define DCPASS_FLAG_OFF(f, b) \
    if ((f) & (b)) { \
        (f) ^= (b); \
    }

/* Loads the decoded character's large picture in the background and fades it in. */
void DcPass_UpdateFace(DcPass *pass) {
    MTexRes *res;

    if (pass->flags & DCPASS_FACE_CHANGE) {
        File_CancelRequests();
        File_Request(pass->status.chara + DCPASS_FACE_FILE, pass->faceFile, DCPASS_FACE_SIZE);
        DCPASS_FLAG_OFF(pass->flags, DCPASS_FACE_CHANGE);
        pass->flags |= DCPASS_FACE_LOADING;
        pass->faceAlpha = 0.0f;
        DCPASS_FLAG_OFF(pass->flags, DCPASS_FACE_READY);
    }
    if (pass->flags & DCPASS_FACE_LOADING) {
        if (File_UpdateRequests()) {
            res = NULL;
            Sprite_Unpack(pass->faceFile, pass->faceRes, NULL);
            res = pass->faceRes;
            Res_RelocateOffsets(&res, res, res);
            pass->faceTex = res->tex;
            DCPASS_FLAG_OFF(pass->flags, DCPASS_FACE_LOADING);
            pass->flags |= DCPASS_FACE_READY;
        }
    }
    if (pass->flags & DCPASS_FACE_READY) {
        if (pass->faceAlpha < 1.0f) {
            pass->faceAlpha += 0.05f;
        }
    }
}

/* Draws the typed text and its cursor. */
void DcPass_DrawTextLine(DcPass *pass) {
    MFlash *flash = &pass->view.flash[0];
    DcPassText *text = &pass->text;

    DcPass_DrawText(flash, text->text);
    DcPass_DrawCursor(flash, text);
}

/* Shows or hides a clip found by name (each use has its own MFlashRef on the stack). */
static inline void DcPass_SetVisible(MFlash *flash, char *parent, char *name, s32 on) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetFlags(flash, &ref, 2, on);
}

/* Sets the picture of a clip found by name. */
static inline void DcPass_SetTex(MFlash *flash, char *parent, char *name, s32 tex) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetTex(flash, &ref, tex);
}

/*
 * Fills the status page of a character: hides the item-slot marks beyond the slots it has, lights the marks of
 * the four bars (minus marks for a negative value, plus marks for a positive one) and sets the attribute icon.
 */
/* Texture rectangle of row i of the 0x80 x 0x14 caption sheet. */
#define DCPASS_ROW(uv, i) DcPass_SetCell(uv, 0, i, 0x80, 0x14)

/*
 * NOT MATCHING: 27 of 233 instructions: the attempt keeps the constant 0x80 of the row rectangle in a saved
 * register from the end of the loop to the rectangle after it, the original loads it again each time (and so
 * has one saved register free: registers shift in that part). Calls, arguments and stores are the same.
 */
#if 0
void DcPass_DrawStatus(DcPassView *view, ZStatus *status) {
    char name[0x100];
    char sub[0x100];
    MFlashUv uv;
    s32 i;
    s32 j;

    for (i = status->val[0]; i < 7; i++) {
        sprintf(name, "mc_status_ability_plus_%d", i + 4);
        DcPass_SetVisible(&view->flash[1], "mc_status_ability_base2_1", name, 0);
    }
    DCPASS_ROW(&uv, i);
    DcPass_SetUv(&view->flash[1], "mc_status_ability_base2_1", "mc_text_ability_&d", &uv);
    for (i = 0; i < 4; i++) {
        s32 value = status->val[i + 1];

        sprintf(name, "mc_status_ability_base1_%d", i + 1);
        for (j = 0; j < 4; j++) {
            sprintf(sub, "mc_status_ability_minus_%d", j + 1);
            DcPass_SetVisible(&view->flash[1], name, sub, 0);
            sprintf(sub, "mc_status_ability_plus_%d", j + 1);
            DcPass_SetVisible(&view->flash[1], name, sub, 0);
            if (value < 0 && value < -j && -j <= 0) {
                sprintf(sub, "mc_status_ability_minus_%d", j + 1);
                DcPass_SetVisible(&view->flash[1], name, sub, 1);
            } else if (value > 0 && j >= 0 && j < value) {
                sprintf(sub, "mc_status_ability_plus_%d", j + 1);
                DcPass_SetVisible(&view->flash[1], name, sub, 1);
            }
        }
        DCPASS_ROW(&uv, i);
        sprintf(name, "mc_status_ability_base1_%d", i);
        DcPass_SetUv(&view->flash[1], name, "mc_text_ability_1", &uv);
    }
    {
        MFlashRef ref;

        Flash_FindLabel(&view->flash[1], NULL, "mc_status_attribute", &ref);
        Flash_ClipSetTex(&view->flash[1], &ref, status->attr);
    }
    DCPASS_ROW(&uv, i);
    sprintf(name, "mc_status_ability_base1_%d", i);
    DcPass_SetUv(&view->flash[1], name, "mc_text_ability_1", &uv);
}
#else
INCLUDE_ASM("asm/nonmatchings/menu/menu_z_d", DcPass_DrawStatus);
#endif
