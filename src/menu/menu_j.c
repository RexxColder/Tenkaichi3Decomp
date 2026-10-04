#include "common.h"
#include "menu/menu_j.h"
#include "sys/pad.h"
#include "sys/save.h"

/*
 * EntrySel, 0x35F650..0x3623A8: rest of the entrant select of Dragon World Tour (progress mode 34) and the
 * handler of modes 33..35. The object starts at 0x35E0F8 (menu_i_d.c: texture helpers, picture loader, Init) and
 * ends here: its .data is the four work pointers at 0x3B5910 and its .rodata runs from 0x3B5920 to 0x3B5DB0.
 */

#define ES_CUR (gEntrySel->sel->entry[gEntrySel->sel->cur])
#define ES_CELL (gEntrySel->grid[ES_CUR.row * ESEL_COLS + ES_CUR.col])

/* Frees everything EntrySel_Init made. */
void EntrySel_Term(void) {
    s32 i;

    func_00399430();
    MsgWin_Term();
    ItemPanel_Term(0);
    TourBg_Term();
    for (i = 0; i < ESEL_FLASH_NUM; i++) {
        Flash_Destroy(&gEntrySel->flash[i]);
    }
    if (gEntrySel->imageFile != NULL) {
        Heap_Free(gEntrySel->imageFile);
        gEntrySel->imageFile = NULL;
    }
    if (gEntrySel->imageRes != NULL) {
        Heap_Free(gEntrySel->imageRes);
        gEntrySel->imageRes = NULL;
    }
    if (gEntrySel->file != NULL) {
        Heap_Free(gEntrySel->file);
        gEntrySel->file = NULL;
    }
    if (gEntrySel->res != NULL) {
        Heap_Free(gEntrySel->res);
        gEntrySel->res = NULL;
    }
    if (gEntrySel != NULL) {
        Heap_Free(gEntrySel);
        gEntrySel = NULL;
    }
}

/* Sets up every clip of the four movies from the current state and draws the screen. */
void EntrySel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    s32 i;
    MFlash *flash;

    TourBg_Draw();
    flash = &gEntrySel->flash[0];
    if (gEntrySel->sel->flags & ESEL_SEL_IMAGE_READY) {
        gEntrySel->imageAlpha += 0.075f;
        if (gEntrySel->imageAlpha >= 1.0f) {
            gEntrySel->imageAlpha = 1.0f;
        }
    } else {
        gEntrySel->imageAlpha = 0.0f;
    }
    Flash_FindLabel(flash, NULL, "mc_single_chara_l", &ref);
    Flash_ClipSetAlpha(flash, &ref, gEntrySel->imageAlpha);
    Flash_FindLabel(flash, NULL, "mc_name_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gEntrySel->sel->image, &gEntrySel->box[0]);
    Flash_FindLabel(flash, NULL, "mc_form_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gEntrySel->sel->image, &gEntrySel->box[1]);
    for (i = 0; i < ESEL_ENTRY_MAX; i++) {
        /* the entry list: "entry n" for those still to choose, the "decided" text for the chosen ones */
        sprintf(name, "mc_entry_text_%d", i);
        if (i >= TOUR_PROG->t.entryNum || i < gEntrySel->sel->cur) {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        } else {
            uv.x0 = (i % 2) * 0x40;
            uv.y0 = (i / 2) * 0x20;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = uv.y0 + 0x20;
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_FindLabel(flash, name, "mc_entry_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_entry_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
        }
        sprintf(name, "mc_entry_decision_text_%d", i);
        if (i < gEntrySel->sel->cur) {
            uv.x0 = (i % 2) * 0x40;
            uv.y0 = (i / 2) * 0x20;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = uv.y0 + 0x20;
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_ClipSetUv(flash, &ref, &uv);
        } else {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    flash = &gEntrySel->flash[2];
    uv.x0 = 0x20;
    uv.y0 = 0;
    uv.x1 = 0x40;
    uv.y1 = 0x20;
    Flash_FindLabel(flash, NULL, "mc_yajirusi", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    for (i = 0; i < 4; i++) {
        /* item set plates: dimmed when they cannot be chosen */
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x100;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_custom_plate_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetColor(flash, &ref, i < gEntrySel->customCount ? 1.0f : 0.3f);
        Flash_FindLabel(flash, name, "mc_custom_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_custom_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (i == 0) {
            Flash_FindLabel(flash, name, "mc_yajirusi", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        } else {
            Flash_FindLabel(flash, name, "mc_yajirusi", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, ES_CELL.id != ESEL_ID_RANDOM);
        }
    }
    for (i = 0; i < 4; i++) {
        /* costume plates */
        uv.x0 = (i % 2) * 0x40;
        uv.y0 = (i / 2) * 0x20;
        uv.x1 = uv.x0 + 0x40;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_color_plate_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (i < gEntrySel->colorCount) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            switch (gEntrySel->colorCount) {
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
    flash = &gEntrySel->flash[1];
    if (TOUR_PROG->t.entryNum - 1 < gEntrySel->sel->cur) {
        uv.x0 = ((gEntrySel->sel->cur - 1) % 2) * 0x40;
        uv.y0 = ((gEntrySel->sel->cur - 1) / 2) * 0x20;
        uv.x1 = uv.x0 + 0x40;
        uv.y1 = uv.y0 + 0x20;
    } else {
        uv.x0 = (gEntrySel->sel->cur % 2) * 0x40;
        uv.y0 = (gEntrySel->sel->cur / 2) * 0x20;
        uv.x1 = uv.x0 + 0x40;
        uv.y1 = uv.y0 + 0x20;
    }
    Flash_FindLabel(flash, NULL, "mc_plate_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gEntrySel->rows >= 2) {
        for (i = 0; i < 2; i++) {
            uv.x0 = i * 0x20;
            uv.y0 = 0x20;
            uv.x1 = uv.x0 + 0x20;
            uv.y1 = 0x40;
            Flash_FindLabel(flash, i != 0 ? "mc_yajirusi_down" : "mc_yajirusi_up",
                            i != 0 ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if ((gEntrySel->sel->flags & ESEL_SEL_FORM) || TOUR_PROG->t.tour == TOUR_YAMCHA) {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            }
        }
    } else {
        for (i = 0; i < 2; i++) {
            Flash_FindLabel(flash, NULL, i != 0 ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    Flash_FindLabel(flash, NULL, "mc_chara_mask", &ref);
    if (gEntrySel->sel->rowAnim != 0) {
        Flash_ClipSetFlags(flash, &ref, 0x102, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 0x102, 0);
    }
    for (i = 0; i < 14; i++) {
        sprintf(name, "mc_chara_chip_%03d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetFlags(flash, &ref, 0x80, (u8)gEntrySel->sel->rowAnim);
    }
    flash = &gEntrySel->flash[3];
    switch (TOUR_PROG->t.tour) {
    case TOUR_WORLD:
        Flash_FindLabel(flash, NULL, "mc_guide_tenkaichi_mouth", &ref);
        FlashAnim_Talk(flash, &ref, &gEntrySel->talk[0], 0);
        break;
    case TOUR_BIG:
        Flash_FindLabel(flash, NULL, "mc_guide_ceru_01_eye", &ref);
        FlashAnim_Blink(flash, &ref, &gEntrySel->blink[0], 0);
        Flash_FindLabel(flash, NULL, "mc_guide_ceru_01_mouth", &ref);
        FlashAnim_Talk(flash, &ref, &gEntrySel->talk[0], 0);
        break;
    case TOUR_CELL:
        Flash_FindLabel(flash, NULL, "mc_guide_ceru_01_eye", &ref);
        FlashAnim_Blink(flash, &ref, &gEntrySel->blink[0], 0);
        Flash_FindLabel(flash, NULL, "mc_guide_ceru_01_mouth", &ref);
        FlashAnim_Talk(flash, &ref, &gEntrySel->talk[0], 0);
        break;
    case TOUR_OTHERWORLD:
        Flash_FindLabel(flash, NULL, "mc_guide_anoyo_mouth", &ref);
        FlashAnim_Talk(flash, &ref, &gEntrySel->talk[0], 0);
        break;
    case TOUR_YAMCHA:
        for (i = 0; i < 2; i++) {
            Flash_FindLabel(flash, NULL, i != 0 ? "mc_guide_puaru_eye01" : "mc_guide_yamucha_eye", &ref);
            FlashAnim_Blink(flash, &ref, &gEntrySel->blink[i], 0);
            Flash_FindLabel(flash, NULL, i != 0 ? "mc_guide_puaru_mouth01" : "mc_guide_yamucha_mouth", &ref);
            if (i == gEntrySel->talker) {
                FlashAnim_Talk(flash, &ref, &gEntrySel->talk[i], 0);
            } else {
                FlashAnim_ShowNext2(flash, &ref, 0);
            }
        }
        break;
    }
    Flash_Draw(&gEntrySel->flash[0]);
    Font_FlushAll();
    Flash_Draw(&gEntrySel->flash[1]);
    Flash_Draw(&gEntrySel->flash[2]);
    MsgWin_Draw(0, 0, gEntrySel->voiceLine);
    Flash_Draw(&gEntrySel->flash[3]);
    ItemPanel_Draw(0);
    func_00399478(gEntrySel->helpItem);
}

/* Advances the movies and the item panel; ends the chip-row scroll when its clip says so. */
void EntrySel_Update(void) {
    s32 i;

    for (i = 0; i < ESEL_FLASH_NUM; i++) {
        Flash_Advance(&gEntrySel->flash[i]);
    }
    ItemPanel_Update(0);
    if (gEntrySel->sel->rowAnim != 0 && (gEntrySel->flash[1].trig & 1)) {
        gEntrySel->sel->rowAnim = 0;
    }
}

/* Pad 0: grid cursor, form, item set, item panel and costume of the entrant being chosen. */
void EntrySel_Input(s32 *result) {
    if (!(gEntrySel->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gEntrySel->flags & ESEL_STARTED)) {
        EntrySel_ClipGoto(1, 0, "fl_on_start");
        EntrySel_ClipGoto(0, 7, "fl_loop");
        gEntrySel->flags |= ESEL_STARTED;
    }
    switch (gEntrySel->sel->step) {
    case ESEL_STEP_CHARA:
        if ((gPad[0].gameRepeat & 1) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(1, 0, "fl_off_start");
            ChrGrid_MoveLeft(gEntrySel->grid, &ES_CUR.col, ES_CUR.row);
            EntrySel_ClipGoto(1, 0, "fl_on_start");
            gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
            EntrySel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if ((gPad[0].gameRepeat & 2) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(1, 0, "fl_off_start");
            ChrGrid_MoveRight(gEntrySel->grid, &ES_CUR.col, ES_CUR.row);
            EntrySel_ClipGoto(1, 0, "fl_on_start");
            gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
            EntrySel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if ((gPad[0].gameRepeat & 8) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            if (gEntrySel->rows >= 2) {
                Flash_GotoLabel(&gEntrySel->flash[1], "fl_reel_down", 1);
                gEntrySel->sel->rowAnim = 1;
                EntrySel_ClipGoto(1, 0, "fl_off_start");
                ChrGrid_MoveUp(gEntrySel->grid, &ES_CUR.col, &ES_CUR.row, gEntrySel->rows);
                EntrySel_ClipGoto(1, 0, "fl_on_start");
                EntrySel_SwapRowTex();
                gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
                EntrySel_ChangeImage();
                Snd_PlaySe(2, 2);
            }
        } else if ((gPad[0].gameRepeat & 4) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            if (gEntrySel->rows >= 2) {
                Flash_GotoLabel(&gEntrySel->flash[1], "fl_reel_up", 1);
                gEntrySel->sel->rowAnim = 1;
                EntrySel_ClipGoto(1, 0, "fl_off_start");
                ChrGrid_MoveDown(gEntrySel->grid, &ES_CUR.col, &ES_CUR.row, gEntrySel->rows);
                EntrySel_ClipGoto(1, 0, "fl_on_start");
                EntrySel_SwapRowTex();
                gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
                EntrySel_ChangeImage();
                Snd_PlaySe(2, 1);
            }
        } else if (gPad[0].gamePressed & 0x200) {
            if (ES_CELL.id == ESEL_ID_RANDOM) {
                /* the random cell: no form, two costumes */
                Flash_GotoLabel(&gEntrySel->flash[2], "fl_custom_in", 1);
                EntrySel_ClipGoto(1, 0, "fl_ok");
                EntrySel_ClipGoto(2, 2, "fl_on_start");
                gEntrySel->customCount = (gEntrySel->flags & ESEL_NO_CUSTOM) ? 1 : 4;
                if (ES_CUR.custom >= gEntrySel->customCount) {
                    ES_CUR.custom = 0;
                }
                gEntrySel->colorCount = 2;
                if (ES_CUR.costume >= gEntrySel->colorCount) {
                    ES_CUR.costume = 0;
                }
                gEntrySel->sel->step = ESEL_STEP_CUSTOM;
                Snd_PlaySe(1, 1);
            } else if (ES_CELL.formCount != 0) {
                gEntrySel->sel->flags |= ESEL_SEL_FORM;
                if (ES_CUR.form >= ES_CELL.formCount) {
                    ES_CUR.form = 0;
                }
                Flash_GotoLabel(&gEntrySel->flash[1], "fl_form", 1);
                gEntrySel->sel->rowAnim = 1;
                EntrySel_ClipGoto(1, 0, "fl_off_start");
                EntrySel_ClipGoto(1, 1, "fl_on_start");
                EntrySel_SetRowTex();
                gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.form];
                EntrySel_ChangeImage();
                gEntrySel->sel->step = ESEL_STEP_FORM;
                Snd_PlaySe(2, 0x29);
            } else {
                Flash_GotoLabel(&gEntrySel->flash[2], "fl_custom_in", 1);
                EntrySel_ClipGoto(1, 0, "fl_ok");
                EntrySel_ClipGoto(2, 2, "fl_on_start");
                gEntrySel->customCount = (gEntrySel->flags & ESEL_NO_CUSTOM) ? 1 : 4;
                if (ES_CUR.custom >= gEntrySel->customCount) {
                    ES_CUR.custom = 0;
                }
                gEntrySel->colorCount = ChrTbl_WrapCostume(gEntrySel->sel->image, &ES_CUR.costume);
                gEntrySel->sel->step = ESEL_STEP_CUSTOM;
                Snd_PlaySe(1, 1);
            }
        } else if (gPad[0].gamePressed & 0x400) {
            if (gEntrySel->sel->cur != 0) {
                /* back to the previous entrant */
                EntrySel_ClipGoto(1, 0, "fl_off_start");
                EntrySel_ClipGoto(0, 7, "fl_stop");
                gEntrySel->sel->cur--;
                EntrySel_SetMemberTex();
                EntrySel_ClipGoto(1, 0, "fl_on_start");
                EntrySel_ClipGoto(0, 7, "fl_loop");
                EntrySel_SwapRowTex();
                gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
                EntrySel_ChangeImage();
            } else {
                ColorFade_StartOut(0, 0, 0, 0x14);
                *result = 0;
            }
            Snd_PlaySe(1, 2);
        }
        break;
    case ESEL_STEP_FORM:
        if ((gPad[0].gameRepeat & 1) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(1, 1, "fl_off_start");
            ChrGrid_PrevForm(gEntrySel->sel->rowChara, &ES_CUR.form);
            EntrySel_ClipGoto(1, 1, "fl_on_start");
            gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.form];
            EntrySel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if ((gPad[0].gameRepeat & 2) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(1, 1, "fl_off_start");
            ChrGrid_NextForm(gEntrySel->sel->rowChara, &ES_CUR.form);
            EntrySel_ClipGoto(1, 1, "fl_on_start");
            gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.form];
            EntrySel_ChangeImage();
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&gEntrySel->flash[2], "fl_custom_in", 1);
            EntrySel_ClipGoto(1, 1, "fl_ok");
            EntrySel_ClipGoto(2, 2, "fl_on_start");
            gEntrySel->customCount = (gEntrySel->flags & ESEL_NO_CUSTOM) ? 1 : 4;
            if (ES_CUR.custom >= gEntrySel->customCount) {
                ES_CUR.custom = 0;
            }
            gEntrySel->colorCount = ChrTbl_WrapCostume(gEntrySel->sel->image, &ES_CUR.costume);
            gEntrySel->sel->step = ESEL_STEP_CUSTOM;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gEntrySel->sel->flags ^= ESEL_SEL_FORM;
            Flash_GotoLabel(&gEntrySel->flash[1], "fl_form", 1);
            gEntrySel->sel->rowAnim = 1;
            EntrySel_ClipGoto(1, 1, "fl_off_start");
            EntrySel_ClipGoto(1, 0, "fl_on_start");
            EntrySel_SwapRowTex();
            gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
            EntrySel_ChangeImage();
            gEntrySel->sel->step = ESEL_STEP_CHARA;
            Snd_PlaySe(2, 0x29);
        }
        break;
    case ESEL_STEP_CUSTOM:
        if ((gPad[0].gameRepeat & 8) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            if (gEntrySel->customCount >= 2) {
                EntrySel_ClipGoto(2, 2, "fl_off_start");
                ES_CUR.custom--;
                if (ES_CUR.custom < 0) {
                    ES_CUR.custom = gEntrySel->customCount - 1;
                }
                EntrySel_ClipGoto(2, 2, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if ((gPad[0].gameRepeat & 4) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            if (gEntrySel->customCount >= 2) {
                EntrySel_ClipGoto(2, 2, "fl_off_start");
                ES_CUR.custom++;
                if (ES_CUR.custom >= gEntrySel->customCount) {
                    ES_CUR.custom = 0;
                }
                EntrySel_ClipGoto(2, 2, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gamePressed & 2) {
            /* look at the items of the set */
            if (ES_CUR.custom > 0 && ES_CELL.id != ESEL_ID_RANDOM) {
                EntrySel_ClipGoto(2, 2, "fl_ok");
                ItemPanel_SetChara(0, gEntrySel->sel->image, ES_CUR.col + ES_CUR.row * ESEL_COLS, ES_CUR.custom - 1,
                                   0);
                ItemPanel_Show(0);
                Flash_GotoLabel(&gEntrySel->flash[3], "fl_guide_out", 1);
                MsgWin_Close();
                gEntrySel->sel->step = ESEL_STEP_PANEL;
            }
        } else if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&gEntrySel->flash[2], "fl_color_in", 1);
            EntrySel_ClipGoto(2, 2, "fl_ok");
            EntrySel_ClipGoto(2, 5, "fl_on_start");
            gEntrySel->sel->step = ESEL_STEP_COLOR;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gEntrySel->flash[2], "fl_custom_cansel", 1);
            EntrySel_ClipGoto(2, 2, "fl_off_start");
            if (gEntrySel->sel->flags & ESEL_SEL_FORM) {
                EntrySel_ClipGoto(1, 1, "fl_on_start");
                gEntrySel->sel->step = ESEL_STEP_FORM;
            } else {
                EntrySel_ClipGoto(1, 0, "fl_on_start");
                gEntrySel->sel->step = ESEL_STEP_CHARA;
            }
            Snd_PlaySe(1, 2);
        }
        break;
    case ESEL_STEP_PANEL: {
        s32 item = ItemPanel_Input(0, 0);

        if (item > 0) {
            func_00399730();
            gEntrySel->helpItem = item - 1;
            gEntrySel->sel->step = ESEL_STEP_HELP;
        } else if (item < 0) {
            ItemPanel_Hide(0);
            EntrySel_ClipGoto(2, 2, "fl_on_start");
            Flash_GotoLabel(&gEntrySel->flash[3], "fl_guide_in", 1);
            MsgWin_Open();
            gEntrySel->sel->step = ESEL_STEP_CUSTOM;
        }
        break;
    }
    case ESEL_STEP_HELP:
        if (gPad[0].gamePressed & 0x600) {
            func_00399760();
            gEntrySel->sel->step = ESEL_STEP_PANEL;
            Snd_PlaySe(1, 2);
        }
        break;
    case ESEL_STEP_COLOR:
        if ((gPad[0].gameRepeat & 1) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(2, 5, "fl_off_start");
            ES_CUR.costume--;
            if (ES_CUR.costume < 0) {
                ES_CUR.costume = gEntrySel->colorCount - 1;
            }
            EntrySel_ClipGoto(2, 5, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gameRepeat & 2) && TOUR_PROG->t.tour != TOUR_YAMCHA) {
            EntrySel_ClipGoto(2, 5, "fl_off_start");
            ES_CUR.costume++;
            if (ES_CUR.costume >= gEntrySel->colorCount) {
                ES_CUR.costume = 0;
            }
            EntrySel_ClipGoto(2, 5, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            /* the entrant is decided: fix the character and take the item set along */
            s32 slot;

            gEntrySel->sel->rowAnim = 0;
            Snd_PlaySe(1, 1);
            Flash_GotoLabel(&gEntrySel->flash[2], "fl_color_ok", 1);
            EntrySel_ClipGoto(2, 5, "fl_ok");
            EntrySel_ClipGoto(0, 7, "fl_stop");
            EntrySel_ClipGoto(2, 2, "fl_off_start");
            if (gEntrySel->sel->flags & ESEL_SEL_FORM) {
                ES_CUR.chara = gEntrySel->sel->rowChara[ES_CUR.form];
                if (ES_CELL.id == ESEL_ID_REC) {
                    ES_CUR.items = TOUR_SAVE->rec[ES_CUR.recCol + ES_CUR.recRow * 7].items;
                } else if (ES_CUR.custom != 0) {
                    ES_CUR.items =
                        *(TourItemSet *)gSaveData->custom[ES_CUR.row * ESEL_COLS + ES_CUR.col].item[ES_CUR.custom - 1];
                } else {
                    memset(&ES_CUR.items, 0, 16);
                }
                EntrySel_ClipGoto(1, 1, "fl_off_start");
            } else if (gEntrySel->sel->flags & ESEL_SEL_REC) {
                /* never reached from this range: nothing sets the flag, and clip kind 6 does not exist */
                ES_CUR.chara = gEntrySel->sel->rowChara[ES_CUR.recCol];
                ES_CUR.items = TOUR_SAVE->rec[ES_CUR.recCol + ES_CUR.recRow * 7].items;
                EntrySel_ClipGoto(1, 6, "fl_off_start");
            } else {
                if (ES_CELL.id == ESEL_ID_RANDOM) {
                    ESelCell *cell;
                    s32 chara;
                    s32 id;

                    do {
                        slot = Rand_Range(gEntrySel->gridCount);
                        id = gEntrySel->grid[slot].id;
                    } while (id >= ESEL_ID_RANDOM);
                    chara = id;
                    if (gEntrySel->grid[slot].formCount != 0) {
                        cell = &gEntrySel->grid[slot];
                        chara = cell->form[Rand_Range(cell->formCount)];
                    }
                    ES_CUR.chara = chara;
                } else {
                    ES_CUR.chara = gEntrySel->sel->rowChara[ES_CUR.col];
                    slot = ES_CUR.col + ES_CUR.row * ESEL_COLS;
                }
                if (ES_CUR.custom != 0) {
                    ES_CUR.items = *(TourItemSet *)gSaveData->custom[slot].item[ES_CUR.custom - 1];
                } else {
                    memset(&ES_CUR.items, 0, 16);
                }
                EntrySel_ClipGoto(1, 0, "fl_off_start");
            }
            if (gEntrySel->sel->flags & ESEL_SEL_FORM) {
                gEntrySel->sel->flags ^= ESEL_SEL_FORM;
            }
            if (gEntrySel->sel->flags & ESEL_SEL_REC) {
                gEntrySel->sel->flags ^= ESEL_SEL_REC;
            }
            gEntrySel->sel->cur++;
            EntrySel_ClipGoto(0, 7, "fl_loop");
            EntrySel_SetMemberTex();
            if (gEntrySel->sel->cur == TOUR_PROG->t.entryNum) {
                Flash_GotoLabel(&gEntrySel->flash[1], "fl_out", 1);
                gEntrySel->endStep = ESEL_END_SPEAK;
            } else {
                EntrySel_ClipGoto(1, 0, "fl_on_start");
                EntrySel_SwapRowTex();
                gEntrySel->sel->image = gEntrySel->sel->rowChara[ES_CUR.col];
                EntrySel_ChangeImage();
                gEntrySel->sel->step = ESEL_STEP_CHARA;
            }
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&gEntrySel->flash[2], "fl_color_cansel", 1);
            EntrySel_ClipGoto(2, 5, "fl_off_start");
            EntrySel_ClipGoto(2, 2, "fl_on_start");
            gEntrySel->sel->step = ESEL_STEP_CUSTOM;
            Snd_PlaySe(1, 2);
        }
        break;
    case 6:
    case 7:
        break;
    }
}

/* Once every entrant is chosen: the guide's closing line, then the leave timer. */
void EntrySel_UpdateEnd(void) {
    if (gEntrySel->endStep == ESEL_END_NONE) {
        return;
    }
    switch (gEntrySel->endStep) {
    case ESEL_END_SPEAK:
        switch (TOUR_PROG->t.tour) {
        case TOUR_WORLD:
            gEntrySel->talker = 0;
            gEntrySel->voiceLine = 0x23;
            break;
        case TOUR_BIG:
            gEntrySel->talker = 0;
            gEntrySel->voiceLine = Rand_Range(2) + 0x7B;
            break;
        case TOUR_CELL:
            gEntrySel->talker = 0;
            gEntrySel->voiceLine = Rand_Range(2) + 0x4E;
            break;
        case TOUR_OTHERWORLD:
            gEntrySel->talker = 0;
            gEntrySel->voiceLine = 0xA0;
            break;
        case TOUR_YAMCHA:
            gEntrySel->talker = Rand_Range(2);
            gEntrySel->voiceLine = (gEntrySel->talker ^ 1) + 0xCB;
            break;
        }
        gEntrySel->endStep++;
        Voice_PlayWithSubtitle(gEntrySel->subtitles, TOUR_VOICE_BASE, gEntrySel->voiceLine);
        break;
    case ESEL_END_WAIT:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gEntrySel->endStep++;
        } else if (gPad[0].gamePressed & 0x200) {
            gEntrySel->endStep++;
            Snd_PlaySe(1, 1);
        }
        break;
    case ESEL_END_LEAVE:
        gEntrySel->flags |= ESEL_DONE;
        gEntrySel->flags |= ESEL_LEAVING;
        gEntrySel->timer = 15;
        gEntrySel->endStep = ESEL_END_NONE;
        break;
    }
}

/*
 * The entrant select (mode 34). Returns 1 when the entrants were chosen (they are then in gProgress->entrant[]),
 * 0 when the player backed out.
 */
s32 EntrySel_Run(s32 section) {
    s32 result = 1;

    EntrySel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        EntrySel_UpdateImage();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            EntrySel_Update();
            EntrySel_UpdateEnd();
        }
        EntrySel_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gEntrySel->flags & ESEL_GREETED) && (gEntrySel->flash[0].flags & MFLASH_PAD)) {
                gEntrySel->flags |= ESEL_GREETED;
                switch (TOUR_PROG->t.tour) {
                case TOUR_WORLD:
                    gEntrySel->voiceLine = 0x22;
                    break;
                case TOUR_BIG:
                    gEntrySel->voiceLine = 0x7A;
                    break;
                case TOUR_CELL:
                    gEntrySel->voiceLine = 0x4D;
                    break;
                case TOUR_OTHERWORLD:
                    gEntrySel->voiceLine = 0x9F;
                    break;
                case TOUR_YAMCHA:
                    gEntrySel->voiceLine = 0xCA;
                    break;
                }
                gEntrySel->talker = 0;
                Voice_PlayWithSubtitle(gEntrySel->subtitles, TOUR_VOICE_BASE, gEntrySel->voiceLine);
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            /* the picture loader must be idle before the buffers are freed */
            if (gEntrySel->loadState != 4) {
                continue;
            }
            break;
        }
        if (gEntrySel->flags & ESEL_LEAVING) {
            if (--gEntrySel->timer == -1) {
                s32 i;

                ColorFade_StartOut(0, 0, 0, 0x14);
                /* hand the entrants to the tournament */
                for (i = 0; i < TOUR_PROG->t.entryNum; i++) {
                    TOUR_PROG->t.entrant[i].chara = gEntrySel->sel->entry[i].chara;
                    TOUR_PROG->t.entrant[i].costume = gEntrySel->sel->entry[i].costume;
                    TOUR_PROG->t.entrant[i].player = i;
                    TOUR_PROG->t.entrant[i].flags |= TOUR_ENT_PLAYER;
                    if (gEntrySel->sel->entry[i].custom != 0) {
                        TOUR_PROG->t.entrant[i].flags |= TOUR_ENT_ITEMS;
                        *(TourItemSet *)TOUR_PROG->t.entrant[i].item = gEntrySel->sel->entry[i].items;
                        if (gEntrySel->grid[gEntrySel->sel->entry[i].row * ESEL_COLS + gEntrySel->sel->entry[i].col]
                                .id == ESEL_ID_REC) {
                            TOUR_PROG->t.entrant[i].flags |= TOUR_ENT_REC;
                        }
                    }
                }
            }
        } else if (gEntrySel->endStep == ESEL_END_NONE) {
            EntrySel_Input(&result);
        }
    }
    EntrySel_Term();
    Dma_ResetBuffers();
    return result;
}

/*
 * Handler of progress modes 33..35, Dragon World Tour: 33 the tournament menu, 34 the entrant select, 35 the
 * tournament itself. Returns 1 to leave the overlay (a battle was set up), 0 to go on dispatching (mode 4, the
 * main menu).
 */
s32 Tour_Main(void) {
    s32 result = 1;
    s32 done = 0;
    /*
     * Always set, and the compiler folds the tests away; they are needed to match. The original had some test
     * inside the two "load the archive" blocks: a label inside the block ends the path of the first CSE pass, so
     * the gProgress address of the stores after it is not shared with the one of the switch (four `lw
     * %lo(gProgress)` against the hoisted `addiu`), which also leaves `result` without a register.
     */
    s32 load = 1;

    do {
        switch (gProgress->mode) {
        case 33:
            if (gMenuArc4 == NULL) {
                if (load) {
                    gMenuArc4 = File_LoadSync(gProgress->baseFile + 2, NULL, 0);
                }
            }
            Bgm_Play(0x10B19);
            if (TourMenu_Run(1)) {
                Adx_StopAll();
                gProgress->mode = 34;
            } else {
                Adx_StopAll();
                gProgress->mode = 4;
                result = 0;
                done = 1;
            }
            break;
        case 34:
            if (gMenuArc4 == NULL) {
                if (load) {
                    gMenuArc4 = File_LoadSync(gProgress->baseFile + 2, NULL, 0);
                }
            }
            Bgm_Play(0x10B1B);
            if (EntrySel_Run(2)) {
                Adx_StopAll();
                gProgress->mode = 35;
                if (gMenuArc4 != NULL) {
                    Heap_Free(gMenuArc4);
                    gMenuArc4 = NULL;
                }
            } else {
                Adx_StopAll();
                gProgress->mode = 33;
            }
            break;
        case 35:
            if (Bracket_Run(0)) {
                done = 1;
                Adx_StopAll();
            } else {
                /* back from the bracket without a battle to play: the mode's clock advances one hour */
                Adx_StopAll();
                gProgress->mode = 33;
                gSaveData->unkA0C++;
                if (gSaveData->unkA0C >= TOUR_HOURS) {
                    gSaveData->unkA0C = 0;
                }
            }
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    if (gMenuArc4 != NULL) {
        Heap_Free(gMenuArc4);
        gMenuArc4 = NULL;
    }
    return result;
}
