#include "common.h"
#include "menu/menu_za.h"

/*
 * Menu overlay DBZP.BIN, 0x3AC440..0x3AE648: tail of the DcPass object, the password entry screen of the Data
 * Center (progress mode 54). The head of the object (the keyboard, the password text, the decoding and the
 * storing of the new character) is in the previous chunk. The work structure is a local variable of DcPass_Run.
 */

void DcPass_ClipBegin(void);
void DcPass_ClipEnd(void);

/* Starts a line of the guide and shows its subtitle. */
static inline void DcPass_Say(DcPass *pass, s32 line) {
    Voice_PlayWithSubtitle(pass->view.subtitles, DC_VOICE_BASE, line);
    pass->voiceLine = line;
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
    uv->x1 = uv->x0 + w;
    uv->y0 = row * h;
    uv->y1 = uv->y0 + h;
}

/* Makes the four plates of the slot list draw inside the list's window. */
void DcPass_SetListClip(DcPassView *view) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash = &view->flash[1];
    s32 i;

    for (i = 0; i < 4; i++) {
        sprintf(name, "mc_menu_plate2_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetCallbackA(flash, &ref, DcPass_ClipBegin, NULL);
        Flash_ClipSetCallbackB(flash, &ref, DcPass_ClipEnd, NULL);
    }
}

/* Hides the up or the down arrow of the slot list. */
void DcPass_HideArrow(DcPassView *view, s32 up) {
    MFlashRef ref;
    char parent[0x100];
    char name[0x100];
    MFlash *flash = &view->flash[1];

    if (up) {
        strcpy(parent, "mc_yajirusi_up");
        strcpy(name, "mc_yajirusi_icon_up");
    } else {
        strcpy(parent, "mc_yajirusi_down");
        strcpy(name, "mc_yajirusi_icon_down");
    }
    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetFlags(flash, &ref, 2, 0);
}

/* Sets the pictures of the two arrows and hides the one that leads nowhere. */
void DcPass_DrawArrows(DcPassView *view, DcPassList *list) {
    MFlashUv uv;
    MFlash *flash = &view->flash[1];
    s32 top;

    DcPass_SetCell(&uv, 0, 1, 0x20, 0x20);
    DcPass_SetUv(flash, "mc_yajirusi_up", "mc_yajirusi_icon_up", &uv);
    DcPass_SetCell(&uv, 1, 1, 0x20, 0x20);
    DcPass_SetUv(flash, "mc_yajirusi_down", "mc_yajirusi_icon_down", &uv);
    top = list->top;
    if (top == 0) {
        DcPass_HideArrow(view, 1);
    } else if (top == DCPASS_REC_NUM - DCPASS_LIST_ROWS) {
        DcPass_HideArrow(view, 0);
    }
}

/* Places the scroll bar's knob. */
void DcPass_DrawScrollBar(DcPassView *view, DcPassList *list) {
    MFlashRef ref;
    MFlash *flash = &view->flash[1];
    s32 range = 0xB5;
    s32 num = DCPASS_REC_NUM;
    s32 y = list->top * range / num;

    Flash_FindLabel(flash, NULL, "mc_scroll_bar_point", &ref);
    Flash_ClipSetScale(flash, &ref, 1.0f, 1.21285701f);
    Flash_ClipSetOffset(flash, &ref, 0, y);
}

/* Lights or dims the plate the list's cursor is on. */
void DcPass_LightRow(DcPassView *view, DcPassList *list, s32 on) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash = &view->flash[1];

    sprintf(name, "mc_menu_plate2_%d", list->cursor + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Keeps the list's first slot in 0..11; returns whether it was in range. */
s32 DcPass_ClampTop(DcPassList *list) {
    if (list->top < 0) {
        list->top = 0;
        return 0;
    }
    if (list->top > DCPASS_REC_NUM - DCPASS_LIST_ROWS) {
        list->top = DCPASS_REC_NUM - DCPASS_LIST_ROWS;
        return 0;
    }
    return 1;
}

/*
 * Fills the four plates of the slot list: the name and form of the character saved there, or a dimmed plate.
 * INCLUDE_ASM: the attempt below is the same code with the saved registers assigned differently (47 of 161
 * instructions: the original keeps the movie in s1, the character in s2, the name buffer in s4, list in s5 and
 * view in s6; this gives s2, s1, s6, s4, s5). Verified with the attempt enabled: every other function of the
 * file matches either way.
 */
#if 0
void DcPass_DrawRows(DcPassView *view, DcPassList *list) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash = &view->flash[1];
    s32 i;
    s32 chara;

    for (i = 0; i < 4; i++) {
        sprintf(name, "mc_menu_plate2_%d", i + 1);
        if (i != 3) {
            chara = list->chara[i + list->top];
            if (i == list->cursor) {
                if (chara == -1) {
                    Flash_FindLabel(flash, NULL, name, &ref);
                    Flash_ClipSetAlpha(flash, &ref, 1.0f);
                    continue;
                }
            } else if (chara == -1) {
                goto empty;
            }
            Flash_FindLabel(flash, name, "mc_menu_text1_on", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, chara, &view->nameBox[i]);
            Flash_FindLabel(flash, name, "mc_menu_text2_on", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, chara, &view->formBox[i]);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetAlpha(flash, &ref, 1.0f);
        } else {
            chara = list->chara[list->extra];
            if (chara == -1) {
            empty:
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetAlpha(flash, &ref, 0.5f);
                continue;
            }
            Flash_FindLabel(flash, name, "mc_menu_text1_on", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, chara, &view->nameBoxB);
            Flash_FindLabel(flash, name, "mc_menu_text2_on", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, chara, &view->formBoxB);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetAlpha(flash, &ref, 1.0f);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/menu/menu_za", DcPass_DrawRows);
#endif

/* Draws the slot list. */
void DcPass_DrawList(DcPass *pass) {
    DcPass_SetListClip(&pass->view);
    DcPass_DrawArrows(&pass->view, &pass->list);
    DcPass_DrawScrollBar(&pass->view, &pass->list);
    DcPass_DrawRows(&pass->view, &pass->list);
}

/* Blinks the guide's eyes and moves her mouth while a voice plays. */
void DcPass_AnimGuide(DcPass *pass) {
    MFlashRef ref;
    MFlash *flash = &pass->view.flash[pass->movie];

    Flash_FindLabel(flash, NULL, "mc_guide_blma_eye", &ref);
    FlashAnim_Blink(flash, &ref, &pass->view.blink, 0);
    Flash_FindLabel(flash, NULL, "mc_guide_blma_mouth", &ref);
    if (Voice_GetStat(0) != MVOICE_IDLE) {
        FlashAnim_Talk(flash, &ref, &pass->view.talk, 0);
    } else {
        FlashAnim_ShowNext2(flash, &ref, 0);
    }
}

/* Scrolls the backdrop pattern of both movies. */
void DcPass_AnimBg(DcPass *pass) {
    MFlashRef ref;
    MFlashUv uv;
    s32 i;

    uv.y0 = 0;
    uv.y1 = 0x40;
    uv.x0 = 0;
    uv.x1 = 0x40;
    for (i = 0; i < DCPASS_FLASH_NUM; i++) {
        Flash_FindLabel(&pass->view.flash[i], NULL, "mc_compane_3", &ref);
        FlashAnim_Scroll(&pass->view.flash[i], &ref, &uv, &pass->view.scroll, NULL, -0.152380944f, 0.0f);
    }
}

/* Puts the new character's name and form name on the two pairs of plates. */
void DcPass_DrawNames(DcPass *pass) {
    MFlashRef ref;
    MFlash *flash = &pass->view.flash[1];
    s32 chara = pass->rec.chara;

    Flash_FindLabel(flash, NULL, "mc_name_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &pass->view.nameBoxL);
    Flash_FindLabel(flash, NULL, "mc_form_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &pass->view.formBoxL);
    Flash_FindLabel(flash, NULL, "mc_name_text_r", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &pass->view.nameBoxR);
    Flash_FindLabel(flash, NULL, "mc_form_text_r", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &pass->view.formBoxR);
}

/* Whether L1 or L2 asks to move the text cursor left. */
s32 DcPass_IsPrevPressed(void) {
    if (gPad[0].status == 0 && (*(u64 *)&gPad[0].gameRepeat & (PADG_L1 | PADG_L2))) {
        return 1;
    }
    return 0;
}

/* Whether R1 or R2 asks to move the text cursor right. */
s32 DcPass_IsNextPressed(void) {
    if (gPad[0].status == 0 && (*(u64 *)&gPad[0].gameRepeat & (PADG_R1 | PADG_R2))) {
        return 1;
    }
    return 0;
}

/* Draws the first blink timer. */
void DcPass_ResetBlink(DcPassView *view) {
    view->blink = Rand_Range(0x20);
}

/* Empties the password text. */
void DcPass_ResetText(DcPassText *text) {
    text->unk50 = -100;
    text->unk58 = 1;
    text->unk54 = -100;
    func_003AB7C8(text);
}

/* Sets the work's starting values. */
void DcPass_Start(DcPass *pass) {
    DcPass_ResetBlink(&pass->view);
    DcPass_ResetText(&pass->text);
    pass->state = DCPASS_ST_TYPE;
    pass->timer = 30;
}

/* Advances both movies. */
void DcPass_Update(DcPass *pass) {
    s32 i;

    for (i = 0; i < DCPASS_FLASH_NUM; i++) {
        Flash_Advance(&pass->view.flash[i]);
    }
}

/*
 * A section of the screen's pack. `host` is the file the section was built from: the development build could
 * read it from the host PC. Nothing uses it here, but the strings are still in the object, in this order.
 */
static inline u8 *DcPass_Section(DcPass *pass, s32 n, const char *host) {
    return MPACK_AT(pass->res, n);
}

#define DP_HOST "host:data/ps2/test/datacenter/password_input/"

#define DP_RES(n, host) \
    res = (MTexRes *)DcPass_Section(pass, n, host); \
    Res_RelocateOffsets(&res, res, res)

/* Unpacks the screen (section `section` of archive 8) and builds its two movies, text boxes and windows. */
void DcPass_Init(DcPass *pass, s32 section) {
    MTexRes *res = NULL;
    s32 i;

    pass->pack = MPACK_AT(gMenuArc8, section);
    pass->res = Sprite_Unpack(pass->pack, NULL, NULL);
    DP_RES(10, DP_HOST "dc_select_bg_PS2_.dbt");
    pass->view.bg = res;
    DP_RES(15, DP_HOST "dc_compane_PS2_.dbt");
    pass->view.tex0[0] = MTEX(res, 0);
    pass->view.tex0[1] = MTEX(res, 1);
    pass->view.tex0[2] = MTEX(res, 2);
    pass->view.tex0[3] = MTEX(res, 3);
    pass->view.tex0[4] = MTEX(res, 4);
    pass->view.tex0[5] = MTEX(res, 5);
    pass->view.tex0[6] = MTEX(res, 6);
    DP_RES(15, DP_HOST "dc_compane_PS2_.dbt");
    pass->view.tex1[0] = MTEX(res, 0);
    pass->view.tex1[1] = MTEX(res, 1);
    pass->view.tex1[2] = MTEX(res, 2);
    pass->view.tex1[3] = MTEX(res, 3);
    pass->view.tex1[4] = MTEX(res, 4);
    pass->view.tex1[5] = MTEX(res, 5);
    pass->view.tex1[6] = MTEX(res, 6);
    DP_RES(16, DP_HOST "pwd_imput_EFFECT_PS2_.dbt");
    pass->view.tex0[24] = MTEX(res, 0);
    pass->view.tex0[25] = MTEX(res, 1);
    pass->view.tex0[26] = MTEX(res, 2);
    DP_RES(16, DP_HOST "pwd_imput_EFFECT_PS2_.dbt");
    pass->view.tex1[30] = MTEX(res, 0);
    pass->view.tex1[31] = MTEX(res, 1);
    pass->view.tex1[32] = MTEX(res, 2);
    DP_RES(11, DP_HOST "dc_select_guide_PS2_.dbt");
    pass->view.tex0[27] = MTEX(res, 0);
    pass->view.tex0[28] = MTEX(res, 1);
    pass->view.tex0[29] = MTEX(res, 3);
    DP_RES(11, DP_HOST "dc_select_guide_PS2_.dbt");
    pass->view.tex1[34] = MTEX(res, 0);
    pass->view.tex1[35] = MTEX(res, 1);
    pass->view.tex1[36] = MTEX(res, 3);
    DP_RES(1, DP_HOST "pwd_imput_1_TEX_PS2_.dbt");
    pass->view.tex0[7] = MTEX(res, 0);
    pass->view.tex0[10] = MTEX(res, 1);
    pass->view.tex0[12] = MTEX(res, 2);
    pass->view.tex0[14] = MTEX(res, 3);
    pass->view.tex0[15] = MTEX(res, 4);
    pass->view.tex0[16] = MTEX(res, 5);
    pass->view.tex0[18] = MTEX(res, 6);
    pass->view.tex0[19] = MTEX(res, 7);
    pass->view.tex0[20] = MTEX(res, 8);
    pass->view.tex0[22] = MTEX(res, 9);
    DP_RES(2, DP_HOST "pwd_imput_1_TEXT_JP_PS2_.dbt");
    pass->view.tex0[8] = MTEX(res, 0);
    pass->view.tex0[9] = MTEX(res, 1);
    pass->view.tex0[11] = MTEX(res, 2);
    pass->view.tex0[13] = MTEX(res, 3);
    pass->view.tex0[17] = MTEX(res, 4);
    pass->view.tex0[21] = MTEX(res, 5);
    pass->view.tex0[23] = MTEX(res, 6);
    Flash_Create(&pass->view.flash[0], DcPass_Section(pass, 17, DP_HOST "password_input_1_PS2_.fod"), pass->view.tex0);
    Flash_Play(&pass->view.flash[0], 1);
    DP_RES(3, DP_HOST "pwd_imput_2_TEX_PS2_.dbt");
    pass->view.tex1[8] = MTEX(res, 0);
    pass->view.tex1[9] = MTEX(res, 1);
    pass->view.tex1[10] = MTEX(res, 2);
    pass->view.tex1[11] = MTEX(res, 3);
    pass->view.tex1[15] = MTEX(res, 4);
    pass->view.tex1[16] = MTEX(res, 5);
    pass->view.tex1[19] = MTEX(res, 6);
    pass->view.tex1[20] = MTEX(res, 7);
    pass->view.tex1[22] = MTEX(res, 8);
    pass->view.tex1[23] = MTEX(res, 9);
    pass->view.tex1[24] = MTEX(res, 10);
    pass->view.tex1[25] = MTEX(res, 11);
    pass->view.tex1[26] = MTEX(res, 12);
    pass->view.tex1[29] = MTEX(res, 13);
    DP_RES(5, DP_HOST "pwd_imput_ball_PS2_.dbt");
    pass->view.tex1[13] = MTEX(res, 0);
    DP_RES(4, DP_HOST "pwd_imput_2_TEXT_JP_PS2_.dbt");
    pass->view.tex1[12] = MTEX(res, 0);
    pass->view.tex1[14] = MTEX(res, 1);
    pass->view.tex1[21] = MTEX(res, 2);
    pass->view.tex1[33] = MTEX(res, 3);
    pass->view.tex1[7] = NULL;
    pass->view.tex1[17] = NULL;
    pass->view.tex1[18] = NULL;
    pass->view.tex1[27] = NULL;
    pass->view.tex1[28] = NULL;
    Flash_Create(&pass->view.flash[1], DcPass_Section(pass, 18, DP_HOST "password_input_2_PS2_.fod"), pass->view.tex1);
    Flash_Play(&pass->view.flash[1], 1);
    pass->view.nameText = DcPass_Section(pass, 13, DP_HOST "chara_name_JP_PS2_.pak");
    pass->view.formText = DcPass_Section(pass, 14, DP_HOST "chara_form_JP_PS2_.pak");
    for (i = 0; i < DCPASS_LIST_ROWS; i++) {
        TextBox_Init(&pass->view.nameBox[i], pass->view.nameText, 2);
        TextBox_SetRect(&pass->view.nameBox[i], 0xAF, 0x1FF, 0x6A, 0x132);
        TextBox_SetUnk80(&pass->view.nameBox[i], 1);
        TextBox_Init(&pass->view.formBox[i], pass->view.formText, 4);
        TextBox_SetRect(&pass->view.formBox[i], 0xAF, 0x1FF, 0x6A, 0x132);
        TextBox_SetUnk80(&pass->view.formBox[i], 1);
    }
    TextBox_Init(&pass->view.nameBoxB, pass->view.nameText, 2);
    TextBox_SetRect(&pass->view.nameBoxB, 0xAF, 0x1FF, 0x6A, 0x132);
    TextBox_SetUnk80(&pass->view.nameBoxB, 1);
    TextBox_Init(&pass->view.formBoxB, pass->view.formText, 4);
    TextBox_SetRect(&pass->view.formBoxB, 0xAF, 0x1FF, 0x6A, 0x132);
    TextBox_SetUnk80(&pass->view.formBoxB, 1);
    TextBox_Init(&pass->view.nameBoxL, pass->view.nameText, 1);
    TextBox_Init(&pass->view.formBoxL, pass->view.formText, 1);
    TextBox_Init(&pass->view.nameBoxR, pass->view.nameText, 2);
    TextBox_Init(&pass->view.formBoxR, pass->view.formText, 2);
    pass->faceFile = Heap_Alloc(DCPASS_FACE_SIZE, 0x40, 0, 2);
    pass->faceRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    File_LoadSync(pass->rec.chara + DCPASS_FACE_FILE, pass->faceFile, DCPASS_FACE_SIZE);
    pass->view.subtitles = DcPass_Section(pass, 9, DP_HOST "dcenter_lips_PS2_.pak");
    pass->view.msgText = DcPass_Section(pass, 8, DP_HOST "dc_msg_JP_PS2_.pak");
    MsgWin_Init(DcPass_Section(pass, 6, DP_HOST "if_msg_window_PS2_.pak"), pass->view.msgText, 0, (s32)pass->view.unk17C);
    pass->view.dialogMsg = DcPass_Section(pass, 21, DP_HOST "font_datacenter_JP_PS2_.pak");
    Dialog_Init(DcPass_Section(pass, 7, DP_HOST "if_system_window_JP_PS2_.pak"), pass->view.dialogMsg, 0);
    pass->itemTbl = DcPass_Section(pass, 12, DP_HOST "zitem_parameter_PS2_.dat");
    PassChk_Init((u32 *)DcPass_Section(pass, 19, DP_HOST "zitem_parameter_PS2_.pak"));
    pass->order = DcPass_Section(pass, 20, DP_HOST "character_select_order_PS2_.dat") + 0x10;
    pass->orderNum = *(s32 *)DcPass_Section(pass, 20, DP_HOST "character_select_order_PS2_.dat");
}

/* Frees everything DcPass_Init made. */
void DcPass_Term(DcPass *pass) {
    s32 i;

    MsgWin_Term();
    Dialog_Term();
    PassChk_Term();
    for (i = 0; i < DCPASS_FLASH_NUM; i++) {
        Flash_Destroy(&pass->view.flash[i]);
    }
    if (pass->res != NULL) {
        Heap_Free(pass->res);
        pass->res = NULL;
    }
    if (pass->faceFile != NULL) {
        Heap_Free(pass->faceFile);
        pass->faceFile = NULL;
    }
    if (pass->faceRes != NULL) {
        Heap_Free(pass->faceRes);
        pass->faceRes = NULL;
    }
}

/* Draws the frame. */
void DcPass_Draw(DcPass *pass) {
    MFlashRef ref;
    MFlash *flash0 = &pass->view.flash[0];
    MFlash *flash1 = &pass->view.flash[1];

    if (!(pass->flags & DCPASS_STARTED)) {
        func_003AB640(&pass->view, &pass->kbd, 1);
        DcPass_LightRow(&pass->view, &pass->list, 1);
        Flash_GotoLabel(flash0, "fl_input_in", 1);
        pass->flags |= DCPASS_STARTED;
    }
    func_003AB780(pass);
    func_003AC050(pass);
    Flash_FindLabel(flash1, NULL, "mc_single_chara_r", &ref);
    Flash_ClipSetAlpha(flash1, &ref, pass->faceAlpha);
    func_003AC098(&pass->view, &pass->rec);
    DcPass_DrawList(pass);
    DcPass_DrawNames(pass);
    DcPass_AnimGuide(pass);
    DcPass_AnimBg(pass);
    Sprite_DrawPicture(pass->view.bg, 0, 0, 0x80);
    Flash_Draw(&pass->view.flash[pass->movie]);
    MsgWin_Draw(0, 0, pass->voiceLine);
    Dialog_Draw(0);
}

/* Plays the "chosen" animation of the plate the list's cursor is on. */
void DcPass_ConfirmRow(DcPass *pass) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash = &pass->view.flash[1];

    sprintf(name, "mc_menu_plate2_%d", pass->list.cursor + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_ok");
}

/* Moves the list's cursor by `step`; returns whether it ran off the three rows. */
static inline s32 DcPass_ClampCursor(DcPassList *list) {
    if (list->cursor < 0) {
        list->cursor = 0;
        return 1;
    }
    if (list->cursor > DCPASS_LIST_ROWS - 1) {
        list->cursor = DCPASS_LIST_ROWS - 1;
        return 1;
    }
    return 0;
}

/* The slot the list's cursor is on. */
static inline s32 DcPass_GetSlot(DcPassList *list) {
    return list->top + list->cursor;
}

/* Changes to the other movie when the one shown asks for it (trigger 0 of its timeline). */
static inline void DcPass_SwitchMovie(DcPass *pass) {
    if (pass->view.flash[0].trig & 1) {
        pass->movie = 1;
        Flash_GotoLabel(&pass->view.flash[1], "fl_new_character", 1);
    } else if (pass->view.flash[1].trig & 1) {
        pass->movie = 0;
        Flash_GotoLabel(&pass->view.flash[0], "fl_input_in", 1);
    }
}

/* Leaves the screen from the keyboard. */
static inline void DcPass_Leave(DcPass *pass) {
    pass->result = 0;
    pass->flags |= DCPASS_LEAVE;
    Flash_GotoLabel(&pass->view.flash[0], "fl_input_out", 1);
}

/* Reads pad 0 for the current state. */
void DcPass_Input(DcPass *pass) {
    s32 ret;

    if (!(pass->view.flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (pass->flags & DCPASS_LEAVE) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    DcPass_SwitchMovie(pass);
    switch (pass->state) {
    case DCPASS_ST_TYPE:
        if (pass->movie == 1) {
            break;
        }
        if (DcPass_IsPrevPressed()) {
            if (pass->text.pos > 0) {
                pass->text.pos--;
                Snd_PlaySe(1, 0);
            }
        } else if (DcPass_IsNextPressed()) {
            if (pass->text.pos < DCPASS_TEXT_LAST) {
                pass->text.pos++;
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].status == 1 && (gPad[0].gameHeld & PADG_SQUARE)) {
            break;
        } else if (gPad[0].gameRepeat & PADG_LEFT) {
            func_003AB640(&pass->view, &pass->kbd, 0);
            do {
                pass->kbd.col--;
            } while (func_003AB038(&pass->kbd, pass->kbd.col + 1));
            func_003AB640(&pass->view, &pass->kbd, 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & PADG_RIGHT) {
            func_003AB640(&pass->view, &pass->kbd, 0);
            do {
                pass->kbd.col++;
            } while (func_003AB038(&pass->kbd, pass->kbd.col - 1));
            func_003AB640(&pass->view, &pass->kbd, 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & PADG_UP) {
            func_003AB640(&pass->view, &pass->kbd, 0);
            do {
                pass->kbd.row--;
            } while (func_003AB088(&pass->kbd, pass->kbd.row + 1));
            func_003AB640(&pass->view, &pass->kbd, 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            func_003AB640(&pass->view, &pass->kbd, 0);
            do {
                pass->kbd.row++;
            } while (func_003AB088(&pass->kbd, pass->kbd.row - 1));
            func_003AB640(&pass->view, &pass->kbd, 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_CIRCLE) {
            pass->kbd.page++;
            func_003AAFA0(&pass->kbd);
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_START) {
            func_003AB640(&pass->view, &pass->kbd, 0);
            func_003AB000(DCPASS_KEY_OK, &pass->kbd);
            func_003AB640(&pass->view, &pass->kbd, 1);
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            func_003AB700(&pass->view, &pass->kbd);
            switch (func_003AAF50(&pass->kbd)) {
            case DCPASS_KEY_PAGE:
                pass->kbd.page++;
                func_003AAFA0(&pass->kbd);
                break;
            case DCPASS_KEY_OK:
                if (func_003ABD28(pass)) {
                    pass->state = DCPASS_ST_SHOWN;
                    Flash_GotoLabel(&pass->view.flash[0], "fl_new_character", 1);
                } else {
                    DcPass_Say(pass, 0x13);
                    pass->state = DCPASS_ST_FAILED;
                    Flash_GotoLabel(&pass->view.flash[0], "fl_failure_in", 1);
                    MsgWin_Open();
                }
                break;
            case DCPASS_KEY_BACK:
                if (func_003AB848(&pass->text)) {
                case DCPASS_KEY_QUIT:
                    DcPass_Leave(pass);
                }
                break;
            case DCPASS_KEY_LEFT:
                if (pass->text.pos > 0) {
                    pass->text.pos--;
                }
                break;
            case DCPASS_KEY_RIGHT:
                if (pass->text.pos < DCPASS_TEXT_LAST) {
                    pass->text.pos++;
                }
                break;
            default:
                if (pass->text.pos == DCPASS_TEXT_LAST) {
                    func_003AB800(&pass->kbd, &pass->text);
                    if (func_003AB8C8(&pass->text)) {
                        func_003AB640(&pass->view, &pass->kbd, 0);
                        func_003AB000(DCPASS_KEY_OK, &pass->kbd);
                        func_003AB640(&pass->view, &pass->kbd, 1);
                    }
                } else {
                    func_003AB800(&pass->kbd, &pass->text);
                }
                break;
            }
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            if (func_003AB848(&pass->text)) {
                DcPass_Leave(pass);
            }
            Snd_PlaySe(1, 2);
        }
        break;
    case DCPASS_ST_FAILED:
        if ((gPad[0].gamePressed & PADG_CROSS) || Voice_GetStat(0) == MVOICE_IDLE) {
            pass->state = DCPASS_ST_TYPE;
            Flash_GotoLabel(&pass->view.flash[0], "fl_failure_out", 1);
            MsgWin_Close();
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
                Voice_StopWithLip();
            }
        }
        break;
    case DCPASS_ST_SHOWN:
        if (gPad[0].gamePressed & PADG_CROSS) {
            if (pass->movie == 1) {
                pass->state = DCPASS_ST_LIST;
                Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_in", 1);
                DcPass_Say(pass, 0x14);
                MsgWin_Open();
                if (gPad[0].gamePressed & PADG_CROSS) {
                    Snd_PlaySe(1, 1);
                }
            }
        }
        break;
    case DCPASS_ST_LIST:
        if ((gPad[0].gameRepeat & PADG_UP) && DcPass_GetSlot(&pass->list) != 0) {
            DcPass_LightRow(&pass->view, &pass->list, 0);
            pass->list.cursor--;
            if (DcPass_ClampCursor(&pass->list)) {
                pass->list.top--;
                if (DcPass_ClampTop(&pass->list)) {
                    Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_down", 1);
                    pass->list.extra = pass->list.top + 3;
                }
            }
            DcPass_LightRow(&pass->view, &pass->list, 1);
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gameRepeat & PADG_DOWN) && DcPass_GetSlot(&pass->list) != DCPASS_REC_NUM - 1) {
            DcPass_LightRow(&pass->view, &pass->list, 0);
            pass->list.cursor++;
            if (DcPass_ClampCursor(&pass->list)) {
                pass->list.top++;
                if (DcPass_ClampTop(&pass->list)) {
                    Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_up", 1);
                    pass->list.extra = pass->list.top - 1;
                }
            }
            DcPass_LightRow(&pass->view, &pass->list, 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            pass->state = DCPASS_ST_ASK_QUIT;
            Dialog_SetMsg(1);
            Dialog_Start(0);
            Dialog_SetCursor(1);
            Dialog_SetChoices(1);
            Voice_StopWithLip();
        } else if ((gPad[0].gamePressed & PADG_LEFT) && pass->list.top + 2 >= 3) {
            pass->list.top -= 3;
            DcPass_ClampTop(&pass->list);
            Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_down", 1);
            pass->list.extra = pass->list.top + 3;
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_RIGHT) && pass->list.top != DCPASS_REC_NUM - DCPASS_LIST_ROWS) {
            pass->list.top += 3;
            DcPass_ClampTop(&pass->list);
            Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_up", 1);
            pass->list.extra = pass->list.top - 1;
            Snd_PlaySe(1, 0);
        }
        if (gPad[0].gamePressed & PADG_CROSS) {
            DcPass_ConfirmRow(pass);
            if ((&pass->list)->chara[DcPass_GetSlot(&pass->list)] == -1) {
                pass->state = DCPASS_ST_STORED;
                func_003ABCA0(&pass->rec, &pass->list);
                DcPass_Say(pass, 0x15);
                Snd_PlaySe(1, 1);
            } else {
                pass->state = DCPASS_ST_ASK_OVER;
                Dialog_SetMsg(0);
                Dialog_Start(0);
                Dialog_SetCursor(1);
                Dialog_SetChoices(1);
            }
        }
        break;
    case DCPASS_ST_ASK_QUIT:
        if (Dialog_IsClosed()) {
            pass->state = pass->nextState;
            if (pass->state == DCPASS_ST_TYPE) {
                Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_out", 1);
                Flash_GotoLabel(&pass->view.flash[1], "fl_input_in", 1);
                MsgWin_Close();
            }
        }
        ret = Dialog_Input(0);
        if (ret == 1) {
            pass->nextState = DCPASS_ST_TYPE;
            Dialog_Start(1);
            Dialog_SetChoices(0);
        } else if (ret == -2) {
            pass->nextState = DCPASS_ST_LIST;
            Dialog_Start(1);
            Dialog_SetChoices(0);
        }
        break;
    case DCPASS_ST_ASK_OVER:
        if (Dialog_IsClosed()) {
            pass->state = pass->nextState;
            if (pass->state == DCPASS_ST_STORED) {
                DcPass_Say(pass, 0x15);
            }
        }
        ret = Dialog_Input(0);
        if (ret == 1) {
            pass->nextState = DCPASS_ST_STORED;
            Dialog_Start(1);
            Dialog_SetChoices(0);
            func_003ABCA0(&pass->rec, &pass->list);
        } else if (ret == -2) {
            pass->nextState = DCPASS_ST_LIST;
            Dialog_Start(1);
            Dialog_SetChoices(0);
        }
        break;
    case DCPASS_ST_STORED:
        if ((gPad[0].gamePressed & PADG_CROSS) || Voice_GetStat(0) == MVOICE_IDLE) {
            if (!(pass->flags & DCPASS_LEAVE)) {
                MsgWin_Close();
                Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_out", 1);
                if (gPad[0].gamePressed & PADG_CROSS) {
                    Snd_PlaySe(1, 1);
                }
            }
            pass->result = 0;
            pass->flags |= DCPASS_LEAVE;
        }
        break;
    }
}

/* Fades out once the screen is left; returns 1 when the fade has finished. */
s32 DcPass_CheckLeave(DcPass *pass) {
    if (pass->flags & DCPASS_LEAVE) {
        if (!(pass->flags & DCPASS_LEAVING)) {
            pass->flags |= DCPASS_LEAVING;
            ColorFade_StartOut(0, 0, 0, 20);
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

/* The password entry screen (progress mode 54). Returns 0. */
s32 DcPass_Run(s32 section) {
    DcPass pass;

    memset(&pass, 0, sizeof(pass));
    DcPass_Start(&pass);
    DcPass_Init(&pass, section);
    ColorFade_StartIn(0, 0, 0, 20);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        DcPass_Update(&pass);
        DcPass_Draw(&pass);
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (DcPass_CheckLeave(&pass)) {
            break;
        }
        DcPass_Input(&pass);
        func_003ABF00(&pass);
    }
    DcPass_Term(&pass);
    Dma_ResetBuffers();
    return pass.result;
}

/* Clip callback: limits drawing to the slot list's window. */
void DcPass_ClipBegin(void) {
    Sprite_SetScissor(0xAF, 0x1FF, 0x6A, 0x132);
}

/* Clip callback: back to the whole screen. */
void DcPass_ClipEnd(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}
