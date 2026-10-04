#include "common.h"
#include "menu/menu_y.h"

/*
 * Menu overlay DBZP.BIN, 0x3A3848..0x3A65E8: tail of the Option object, the option screen of mode 62 (the
 * object's head, Option_Init / Option_Run / Option_Input, is in the previous chunk, src/menu/menu_x_c.c; this
 * file appends to it).
 *
 * Object boundary: the read-only data of these functions runs on from the head's without a gap (0x3BC520
 * "mc_dende_eye" follows Option_Input's last jump table) and ends with Option_UpdateReset's jump table at
 * 0x3BC924; the next address, 0x3BC928, is initialised data of the next link group. "fl_on_start" and
 * "fl_off_start" are strings that the head already emitted, so they are referenced by address here.
 */

#define OPT_STR_ON_START D_003BC370
#define OPT_STR_OFF_START D_003BC380

/*
 * Option_Draw is INCLUDE_ASM: the attempt below has the same 2153 instructions and differs in 14 of them, all
 * register choices (saved registers s2 / s3 swapped between the hoisted constants 0x80 and 0x200 of the
 * "fromPage == 1" loop, and between pickY and the address of gOption in the first picker loop). Behaviour is
 * the same. The order of the `uv` stores in each block was found by search (build/scratch_menu_y/perm.py).
 */
#if 0
#define DEFAULT_KEYS_LOOP() \
                    for (i = 0; i < 8; i++) { \
                        uv.y0 = 0; \
                        uv.x0 = 0; \
                        switch (gOption->key[gOption->pad][i]) { \
                            case 2: \
                                uv.y0 = 0x50; \
                                uv.x0 = 0x80; \
                                break; \
                            case 1: \
                                uv.y0 = 0x78; \
                                uv.x0 = 0x80; \
                                break; \
                            case 0: \
                                uv.y0 = 0x28; \
                                uv.x0 = 0x80; \
                                break; \
                            case 3: \
                                uv.y0 = 0; \
                                uv.x0 = 0x80; \
                                break; \
                            case 4: \
                                break; \
                            case 5: \
                                uv.x0 = 0; \
                                uv.y0 = 0x28; \
                                break; \
                            case 6: \
                                uv.x0 = 0; \
                                uv.y0 = 0x50; \
                                break; \
                            case 7: \
                                uv.x0 = 0; \
                                uv.y0 = 0x78; \
                                break; \
                        } \
                        switch (i) { \
                            case 5: \
                                text = 0; \
                                break; \
                            case 4: \
                                text = 1; \
                                break; \
                            case 7: \
                                text = 2; \
                                break; \
                            case 6: \
                                text = 3; \
                                break; \
                            case 3: \
                                text = 4; \
                                break; \
                            case 2: \
                                text = 5; \
                                break; \
                            case 0: \
                                text = 6; \
                                break; \
                            case 1: \
                                text = 7; \
                                break; \
                        } \
                        uv.y1 = uv.y0 + 0x28; \
                        uv.x1 = uv.x0 + 0x80; \
                        sprintf(name, "mc_button_text_%d", text); \
                        Flash_FindLabel(flash, NULL, name, &ref); \
                        Flash_ClipSetUv(flash, &ref, &uv); \
                    }

/* Per frame: sets up every clip of the movie from the screen's state and draws the screen. */
void Option_Draw(void) {
    MFlashRef ref;
    MFlashRef eye;
    MFlashRef mouth;
    char name[0x40];
    MFlashUv uv;
    MFlash *flash;
    s32 i;
    s32 text = 0;
    s32 num = 0;
    s32 y;
    s32 rows4;
    s32 typeY;
    s32 prevY;
    s32 bgmY;
    s32 pickY;
    s32 ctrlY;
    s32 row;
    s32 col;
    s32 id;
    f32 scale;
    f32 fy;
    f32 rows;
    f32 len;

    flash = gOption->flash;
    Sprite_DrawPicture(gOption->bg, 0, 0, 0x80);
    Flash_Advance(gOption->flash);
    Flash_FindLabel(flash, NULL, "mc_dende_eye", &eye);
    Flash_FindLabel(flash, NULL, "mc_dende_mouth", &mouth);
    FlashAnim_Blink(flash, &eye, &gOption->blink, 0);
    FlashAnim_Talk(flash, &mouth, &gOption->talk, 0);

    /* the icon of the page the cursor is in */
    if (gOption->state == YOPT_SCREEN || (gOption->state >= YOPT_TYPE && gOption->state <= YOPT_ADJUST_RESET)) {
        IconWin_SetIcon(1);
    } else if (gOption->state == YOPT_SOUND ||
               (gOption->state >= YOPT_STEREO && gOption->state <= YOPT_SND_RESET && gOption->state != YOPT_BGM)) {
        IconWin_SetIcon(2);
    } else if (gOption->state == YOPT_BGM) {
        IconWin_SetIcon(3);
    } else if (gOption->state == YOPT_CTRL || gOption->state >= YOPT_KEYS) {
        IconWin_SetIcon(4);
    } else {
        IconWin_SetIcon(0);
    }

    switch (gOption->page) {
        case 0:
            rows4 = 4;
            for (i = 0; i < rows4; i++) {
                uv.x0 = 0;
                uv.y0 = i * 0x20;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x20;
                sprintf(name, "mc_menu_plate_%d", i + 1);
                Option_SetMenuText(flash, &ref, name, uv);
            }
            if (gOption->fromPage == 1) {
                for (i = 0; i < 3; i++) {
                    uv.x0 = 0;
                    uv.y0 = i * 0x20 + (rows4 << 5);
                    uv.x1 = 0x200;
                    uv.y1 = i * 0x20 + 0xA0;
                    sprintf(name, "mc_menu_plate_%d", i + 5);
                    Option_SetMenuText(flash, &ref, name, uv);
                }
                sprintf(name, "mc_menu_plate_%d", 8);
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else if (gOption->fromPage == 2 || gOption->fromPage == 3) {
                uv.x0 = 0;
                uv.y0 = 0xE0;
                uv.y1 = 0x100;
                uv.x1 = 0x200;
                sprintf(name, "mc_menu_plate_%d", 5);
                Option_SetMenuText(flash, &ref, name, uv);
                for (i = 0; i < 3; i++) {
                    uv.x0 = 0;
                    uv.y0 = i * 0x20;
                    uv.x1 = 0x200;
                    uv.y1 = i * 0x20 + 0x20;
                    sprintf(name, "mc_menu_plate_%d", i + 6);
                    Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
                    Flash_ClipSetUv(flash, &ref, &uv);
                    Flash_ClipSetTex(flash, &ref, 1);
                    Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
                    Flash_ClipSetUv(flash, &ref, &uv);
                    Flash_ClipSetTex(flash, &ref, 1);
                }
                if (gOption->fromPage == 2) {
                    sprintf(name, "mc_menu_plate_%d", 8);
                    Flash_FindLabel(flash, NULL, name, &ref);
                    Flash_ClipSetFlags(flash, &ref, 2, 0);
                }
            } else {
                for (i = 0; i < 2; i++) {
                    uv.x0 = 0;
                    uv.y0 = i * 0x20 + 0x60;
                    uv.x1 = 0x200;
                    uv.y1 = i * 0x20 + 0x80;
                    sprintf(name, "mc_menu_plate_%d", i + 5);
                    Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
                    Flash_ClipSetUv(flash, &ref, &uv);
                    Flash_ClipSetTex(flash, &ref, 1);
                    Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
                    Flash_ClipSetUv(flash, &ref, &uv);
                    Flash_ClipSetTex(flash, &ref, 1);
                }
                sprintf(name, "mc_menu_plate_%d", 7);
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
                sprintf(name, "mc_menu_plate_%d", 8);
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            uv.y0 = 0;
            uv.y1 = 0x20;
            uv.x0 = 0;
            uv.x1 = 0x100;
            sprintf(name, "mc_bottom_plate_%d", 1);
            Flash_FindLabel(flash, name, "mc_bottom_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_bottom_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            uv.x0 = 0;
            uv.y0 = 0x20;
            uv.y1 = 0x40;
            uv.x1 = 0x100;
            sprintf(name, "mc_bottom_plate_%d", 2);
            Option_SetBottomText(flash, &ref, name, uv);
            break;
        case 1:
            for (i = 0; i < 3; i++) {
                uv.x0 = 0;
                uv.y0 = i * 0x20 + 0x80;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0xA0;
                sprintf(name, "mc_menu_plate_%d", i + 1);
                Option_SetMenuText(flash, &ref, name, uv);
            }
            sprintf(name, "mc_menu_plate_%d", 4);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            for (i = 0; i < 4; i++) {
                uv.x0 = 0;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x20;
                uv.y0 = i * 0x20;
                sprintf(name, "mc_menu_plate_%d", i + 5);
                Option_SetMenuText(flash, &ref, name, uv);
            }
            Flash_FindLabel(flash, NULL, "mc_layout_bg", &ref);
            Flash_ClipSetTex(flash, &ref, gSaveData->unk1694 * 2);
            Flash_FindLabel(flash, NULL, "mc_layout_window", &ref);
            Flash_ClipSetTex(flash, &ref, 0);
            uv.x0 = 0;
            uv.y0 = 0x20;
            uv.x1 = 0x100;
            uv.y1 = 0x40;
            sprintf(name, "mc_bottom_plate_%d", 1);
            Option_SetBottomText(flash, &ref, name, uv);
            uv.y0 = 0;
            uv.x0 = 0;
            uv.x1 = 0x100;
            uv.y1 = 0x20;
            sprintf(name, "mc_bottom_plate_%d", 2);
            Option_SetBottomText(flash, &ref, name, uv);
            uv.y0 = 0;
            uv.y1 = 0x100;
            uv.x0 = 0;
            uv.x1 = 0x100;
            sprintf(name, "mc_type_image_%d", 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            typeY = gOption->type * 0x28;
            uv.y0 = typeY;
            uv.x0 = 0;
            uv.x1 = 0x200;
            uv.y1 = typeY + 0x28;
            sprintf(name, "mc_type_text_%d", 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            prevY = gOption->typePrev * 0x28;
            uv.y0 = prevY;
            uv.x0 = 0;
            uv.x1 = 0x200;
            uv.y1 = prevY + 0x28;
            sprintf(name, "mc_type_text_%d", 2);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            uv.y0 = 0;
            uv.y1 = 0x100;
            uv.x1 = 0x100;
            uv.x0 = 0;
            sprintf(name, "mc_type_image_%d", 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, gOption->type);
            sprintf(name, "mc_type_image_%d", 2);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, gOption->typePrev);
            break;
        case 2:
            uv.x0 = 0;
            uv.y0 = 0xE0;
            uv.y1 = 0x100;
            uv.x1 = 0x200;
            sprintf(name, "mc_menu_plate_%d", 1);
            Option_SetMenuText(flash, &ref, name, uv);
            for (i = 0; i < 3; i++) {
                uv.x0 = 0;
                uv.y0 = i * 0x20;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x20;
                sprintf(name, "mc_menu_plate_%d", i + 2);
                Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetTex(flash, &ref, 1);
                Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetTex(flash, &ref, 1);
            }
            for (i = 0; i < 4; i++) {
                uv.x0 = 0;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x20;
                uv.y0 = i * 0x20;
                sprintf(name, "mc_menu_plate_%d", i + 5);
                Option_SetMenuText(flash, &ref, name, uv);
                Flash_ClipSetTex(flash, &ref, 0);
            }
            uv.y0 = 0x20;
            uv.x0 = 0;
            uv.x1 = 0x100;
            uv.y1 = 0x40;
            sprintf(name, "mc_bottom_plate_%d", 1);
            Option_SetBottomText(flash, &ref, name, uv);
            uv.y0 = 0;
            uv.x0 = 0;
            uv.y1 = 0x20;
            uv.x1 = 0x100;
            sprintf(name, "mc_bottom_plate_%d", 2);
            Option_SetBottomText(flash, &ref, name, uv);
            break;
        case 3:
            for (i = 0; i < 2; i++) {
                uv.x0 = 0;
                uv.y0 = i * 0x20 + 0x60;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x80;
                sprintf(name, "mc_menu_plate_%d", i + 1);
                Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetTex(flash, &ref, 1);
                Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetTex(flash, &ref, 1);
            }
            for (i = 0; i < 4; i++) {
                uv.x0 = 0;
                uv.x1 = 0x200;
                uv.y1 = i * 0x20 + 0x20;
                uv.y0 = i * 0x20;
                sprintf(name, "mc_menu_plate_%d", i + 5);
                Option_SetMenuText(flash, &ref, name, uv);
            }
            sprintf(name, "mc_menu_plate_%d", 3);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            sprintf(name, "mc_menu_plate_%d", 4);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            uv.y0 = 0x20;
            uv.x0 = 0;
            uv.x1 = 0x100;
            uv.y1 = 0x40;
            sprintf(name, "mc_bottom_plate_%d", 1);
            Flash_FindLabel(flash, name, "mc_bottom_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_bottom_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            uv.y0 = 0;
            uv.y1 = 0x20;
            uv.x0 = 0;
            uv.x1 = 0x100;
            sprintf(name, "mc_bottom_plate_%d", 2);
            Flash_FindLabel(flash, name, "mc_bottom_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_bottom_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);

            /* the eight key names: the assignment being edited */
            uv.y0 = 0;
            uv.y1 = 0xA0;
            uv.x0 = 0x30;
            uv.x1 = 0x60;
            Flash_FindLabel(flash, NULL, "mc_button_mark_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            for (i = 0; i < 8; i++) {
                uv.y0 = 0;
                uv.x0 = 0;
                switch (gSaveData->keyEdit[gOption->pad][i]) {
                    case 2:
                        uv.y0 = 0x50;
                        uv.x0 = 0x80;
                        break;
                    case 1:
                        uv.y0 = 0x78;
                        uv.x0 = 0x80;
                        break;
                    case 0:
                        uv.y0 = 0x28;
                        uv.x0 = 0x80;
                        break;
                    case 3:
                        uv.y0 = 0;
                        uv.x0 = 0x80;
                        break;
                    case 4:
                        break;
                    case 5:
                        uv.x0 = 0;
                        uv.y0 = 0x28;
                        break;
                    case 6:
                        uv.x0 = 0;
                        uv.y0 = 0x50;
                        break;
                    case 7:
                        uv.x0 = 0;
                        uv.y0 = 0x78;
                        break;
                }
                switch (i) {
                    case 0:
                        text = 6;
                        break;
                    case 1:
                        text = 7;
                        break;
                    case 2:
                        text = 5;
                        break;
                    case 3:
                        text = 4;
                        break;
                    case 4:
                        text = 1;
                        break;
                    case 5:
                        text = 0;
                        break;
                    case 6:
                        text = 3;
                        break;
                    case 7:
                        text = 2;
                        break;
                }
                uv.y1 = uv.y0 + 0x28;
                uv.x1 = uv.x0 + 0x80;
                sprintf(name, "mc_button_text_%d", text);
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
            }

            /* the default assignment instead, while the page offers it */
            if (gOption->state == YOPT_KEYS || gOption->state == YOPT_KEYS_PAD || gOption->state == YOPT_CTRL) {
                do {
                    if (gOption->state == YOPT_KEYS || gOption->state == YOPT_CTRL) {
                        if (gOption->value == 0) {
                            if (gSaveData->flags & 8) {
                                break;
                            }
                        } else if (gSaveData->flags & 0x10) {
                            break;
                        }
                    }
                    if (gOption->state == YOPT_KEYS_PAD) {
                        if (gOption->value != 0) {
                            break;
                        }
                    }
                    DEFAULT_KEYS_LOOP();
                } while (0);
            }

            /* the mark on the row being edited */
            if (gOption->keyEdit) {
                switch (gOption->keyRow) {
                    case 0:
                        uv.y0 = 0x50;
                        uv.x0 = 0x30;
                        break;
                    case 1:
                        uv.y0 = 0x78;
                        uv.x0 = 0x30;
                        break;
                    case 2:
                        uv.x0 = 0;
                        uv.y0 = 0x78;
                        break;
                    case 3:
                        uv.x0 = 0;
                        uv.y0 = 0x50;
                        break;
                    case 4:
                        uv.x0 = 0;
                        uv.y0 = 0x28;
                        break;
                    case 5:
                        uv.y0 = 0;
                        uv.x0 = 0;
                        break;
                    case 6:
                        uv.y0 = 0x28;
                        uv.x0 = 0x30;
                        break;
                    case 7:
                        uv.y0 = 0;
                        uv.x0 = 0x30;
                        break;
                }
                uv.y1 = uv.y0 + 0x28;
                uv.x1 = uv.x0 + 0x30;
                Flash_FindLabel(flash, NULL, "mc_button_mark_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetOffset(flash, &ref, gOption->markX, gOption->markY);
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_FindLabel(flash, NULL, "mc_button_mark_on", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            break;
    }

    /* the two-way picker's captions */
    if (gOption->picker == 4 || gOption->picker == 2) {
        pickY = 0;
    } else if (gOption->picker == 0) {
        pickY = 0x40;
    } else if (gOption->picker == 3) {
        pickY = 0x80;
    } else {
        pickY = 0xC0;
    }
    for (i = 0; i < 2; i++) {
        uv.x0 = 0;
        uv.y1 = pickY + i * 0x20 + 0x20;
        uv.x1 = 0x100;
        uv.y0 = pickY + i * 0x20;
        sprintf(name, "mc_select_plate_%d", i + 1);
        if (gOption->picker < 7) {
            Flash_FindLabel(flash, name, "mc_select_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if (gOption->picker == 2) {
                Flash_ClipSetTex(flash, &ref, 1);
            }
            Flash_FindLabel(flash, name, "mc_select_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if (gOption->picker == 2) {
                Flash_ClipSetTex(flash, &ref, 1);
            }
        } else {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    ctrlY = 0;
    if (gOption->ctrlKind) {
        ctrlY = 0x40;
    }
    for (i = 0; i < 2; i++) {
        uv.y0 = ctrlY + i * 0x20;
        uv.y1 = ctrlY + i * 0x20 + 0x20;
        uv.x1 = 0x100;
        uv.x0 = 0;
        sprintf(name, "mc_select_plate_%d", i + 3);
        if (gOption->picker != 3 && gOption->picker != 4 && gOption->picker != 0 && gOption->picker != 6 &&
            gOption->picker < 7) {
            Flash_FindLabel(flash, name, "mc_select_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, gOption->ctrlKind != 0);
            Flash_FindLabel(flash, name, "mc_select_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, gOption->ctrlKind != 0);
        } else {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }

    /* the volume window: ten numbered plates per volume */
    for (i = 0; i < 3; i++) {
        for (col = 0; col < 4; col++) {
            uv.y0 = i * 0x20;
            uv.x0 = col * 0x20;
            uv.x1 = col * 0x20 + 0x20;
            uv.y1 = i * 0x20 + 0x20;
            sprintf(name, "mc_vol_plate_bgm_%d", num);
            if (gOption->picker < 7) {
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_FindLabel(flash, name, "mc_vol_num_off", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_FindLabel(flash, name, "mc_vol_num_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
            }
            sprintf(name, "mc_vol_plate_se_%d", num);
            if (gOption->picker < 7) {
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_FindLabel(flash, name, "mc_vol_num_off", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_FindLabel(flash, name, "mc_vol_num_on", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
            }
            num++;
            if (num >= 10) {
                break;
            }
        }
    }
    if (gOption->picker < 7) {
        Flash_FindLabel(flash, NULL, "mc_vol_now_bgm", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
        Flash_FindLabel(flash, NULL, "mc_vol_now_se", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
        Flash_FindLabel(flash, NULL, "mc_vol_window", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }

    /* the sound test's list: six rows and the one scrolling out */
    if (gOption->state == YOPT_BGM) {
        for (i = 0; i < 6; i++) {
            sprintf(name, "mc_bgm_plate_%d", i + 1);
            if (i == 0 || i == 5) {
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipSetCallbackA(flash, &ref, Option_SetBgmScissor, NULL);
                Flash_ClipSetCallbackB(flash, &ref, Option_ResetScissor, NULL);
            }
            bgmY = gOption->bgmIds[gOption->bgmTop + i] % 8 * 0x20;
            uv.y0 = bgmY;
            uv.x0 = 0;
            uv.x1 = 0x200;
            uv.y1 = bgmY + 0x20;
            uv.unk10 = gOption->bgmIds[gOption->bgmTop + i] / 8;
            sprintf(name, "mc_bgm_plate_%d", i + 1);
            Flash_FindLabel(flash, name, "mc_bgm_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, uv.unk10);
            Flash_FindLabel(flash, name, "mc_bgm_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetTex(flash, &ref, uv.unk10);
        }
        Flash_FindLabel(flash, NULL, "mc_bgm_plate_7", &ref);
        Flash_ClipSetCallbackA(flash, &ref, Option_SetBgmScissor, NULL);
        Flash_ClipSetCallbackB(flash, &ref, Option_ResetScissor, NULL);
        rows = 6.0f;
        bgmY = gOption->bgmIds[gOption->bgmExtra] % 8 * 0x20;
        uv.y1 = bgmY + 0x20;
        uv.x1 = 0x200;
        uv.y0 = bgmY;
        uv.x0 = 0;
        uv.unk10 = gOption->bgmIds[gOption->bgmExtra] / 8;
        Flash_FindLabel(flash, "mc_bgm_plate_7", "mc_bgm_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetTex(flash, &ref, uv.unk10);
        Flash_FindLabel(flash, "mc_bgm_plate_7", "mc_bgm_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetTex(flash, &ref, uv.unk10);
        len = 7.3f;
        y = 255.49999f / gOption->bgmCount * gOption->bgmTop;
        scale = rows / gOption->bgmCount * len;
        Flash_FindLabel(flash, NULL, "mc_bgm_scroll_bar", &ref);
        Flash_ClipSetScale(flash, &ref, 1.0f, scale);
        Flash_ClipSetOffset(flash, &ref, 0, y);
    }

    /* the volume marks and the arrows */
    uv.y0 = 0;
    uv.x0 = 0;
    uv.x1 = 0x40;
    uv.y1 = 0x80;
    Flash_FindLabel(flash, NULL, "mc_vol_now_bgm", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetOffset(flash, &ref, gOption->bgmVolume * 0x23, 0);
    Flash_FindLabel(flash, NULL, "mc_vol_now_se", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_ClipSetOffset(flash, &ref, gOption->seVolume * 0x23, 0);
    Flash_FindLabel(flash, "mc_yajirusi_up", "mc_yajirusi_icon_up", &ref);
    Flash_ClipSetFlags(flash, &ref, 2, gOption->bgmTop != 0);
    Flash_FindLabel(flash, "mc_yajirusi_down", "mc_yajirusi_icon_down", &ref);
    Flash_ClipSetFlags(flash, &ref, 2, gOption->bgmBottom < gOption->bgmCount);
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    uv.x0 = 0;
    uv.x1 = 0x20;
    Flash_FindLabel(flash, "mc_yajirusi_up", "mc_yajirusi_icon_up", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    Flash_FindLabel(flash, "mc_yajirusi_down", "mc_yajirusi_icon_down", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.y0 = 0;
    uv.y1 = 0x20;
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    Flash_FindLabel(flash, "mc_yajirusi_right", "mc_yajirusi_icon_right", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gOption->state == YOPT_VOLUME && gOption->value == 1) {
        Flash_ClipSetOffset(flash, &ref, 0, 0x70);
        Flash_FindLabel(flash, NULL, "mc_yajirusi_left", &ref);
        Flash_ClipSetOffset(flash, &ref, 0, 0x70);
    } else if (gOption->state == YOPT_KEYS_EDIT && gOption->keyHeld) {
        Flash_FindLabel(flash, NULL, "mc_yajirusi_right", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 1);
        Flash_ClipSetOffset(flash, &ref, gOption->markX, gOption->markY);
        Flash_FindLabel(flash, NULL, "mc_yajirusi_left", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 1);
        Flash_ClipSetOffset(flash, &ref, gOption->markX, gOption->markY);
    } else {
        Flash_ClipSetOffset(flash, &ref, 0, 0);
        Flash_FindLabel(flash, NULL, "mc_yajirusi_left", &ref);
        Flash_ClipSetOffset(flash, &ref, 0, 0);
    }
    if (gOption->state != YOPT_VOLUME && gOption->state != YOPT_TYPE && !gOption->keyHeld) {
        Flash_FindLabel(flash, NULL, "mc_yajirusi_left", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
        Flash_FindLabel(flash, NULL, "mc_yajirusi_right", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }

    for (i = 0; i < OPTION_FLASH_NUM; i++) {
        Flash_Draw(&gOption->flash[i]);
    }
    IconWin_Draw();
    MsgWin_Draw(0, 0, gOption->voiceLine);
    ColorFade_Draw();
    if (gOption->mcBusy) {
        gOption->mcBusy = McFlow_Update();
        if (!gOption->mcBusy) {
            Option_PlateGoto(0, OPT_CLIP_PICK, OPT_STR_ON_START);
        }
    } else {
        Dialog_Draw(1);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/menu/menu_y", Option_Draw);
#endif

/* Sends one of the screen's plates to a label of its timeline. */
void Option_PlateGoto(s32 unused, s32 clip, char *label) {
    MFlashRef ref;
    char name[0x40];
    MFlash *flash = gOption->flash;

    switch (clip) {
        case OPT_CLIP_ROW:
            sprintf(name, "mc_menu_plate_%d", gOption->cursor);
            break;
        case OPT_CLIP_ROW_SCREEN:
            sprintf(name, "mc_menu_plate_%d", gOption->cursor - 6);
            break;
        case OPT_CLIP_ROW_SOUND:
            sprintf(name, "mc_menu_plate_%d", gOption->cursor - 11);
            break;
        case OPT_CLIP_BOTTOM:
            sprintf(name, "mc_bottom_plate_%d", 1);
            break;
        case OPT_CLIP_PICK:
            sprintf(name, "mc_select_plate_%d", gOption->value + 1);
            break;
        case OPT_CLIP_VIB_OFF:
            sprintf(name, "mc_select_plate_%d",
                    gOption->pad == 0 ? ((gSaveData->flags & 2) ? 3 : 4) : ((gSaveData->flags & 4) ? 3 : 4));
            break;
        case OPT_CLIP_KEY_OFF:
            sprintf(name, "mc_select_plate_%d",
                    gOption->pad == 0 ? ((gSaveData->flags & 8) ? 4 : 3) : ((gSaveData->flags & 0x10) ? 4 : 3));
            break;
        case OPT_CLIP_BGM:
            sprintf(name, "mc_bgm_plate_%d", gOption->bgmCursor - gOption->bgmTop + 1);
            break;
        case OPT_CLIP_VOL_BGM:
            sprintf(name, "mc_vol_plate_bgm_%d", gSaveData->bgmVolume);
            break;
        case OPT_CLIP_VOL_SE:
            sprintf(name, "mc_vol_plate_se_%d", gSaveData->seVolume);
            break;
        case OPT_CLIP_ROW_CTRL:
            sprintf(name, "mc_menu_plate_%d", gOption->cursor - 16);
            break;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Key page: where the cursor mark and the arrows go for a row of the key table; returns the row. */
s32 Option_SetKeyMark(s32 row) {
    s32 ret = 0;

    switch (row) {
        case 0:
            ret = 0;
            gOption->markX = 0xB4;
            gOption->markY = 0x50;
            gOption->arrowX = 0xB4;
            gOption->arrowY = 0x50;
            break;
        case 1:
            ret = 1;
            gOption->markX = 0xB4;
            gOption->markY = 0x78;
            gOption->arrowX = 0xB4;
            gOption->arrowY = 0x78;
            break;
        case 2:
            ret = 2;
            gOption->markX = 0;
            gOption->markY = 0x78;
            gOption->arrowX = 0;
            gOption->arrowY = 0x78;
            break;
        case 3:
            ret = 3;
            gOption->markX = 0;
            gOption->markY = 0x50;
            gOption->arrowX = 0;
            gOption->arrowY = 0x50;
            break;
        case 4:
            ret = 4;
            gOption->markX = 0;
            gOption->markY = 0x28;
            gOption->arrowX = -0xB4;
            gOption->arrowY = 0;
            break;
        case 5:
            ret = 5;
            gOption->markX = 0;
            gOption->markY = 0;
            gOption->arrowX = -0xB4;
            gOption->arrowY = -0x28;
            break;
        case 6:
            ret = 6;
            gOption->markX = 0xB4;
            gOption->markY = 0x28;
            gOption->arrowX = 0;
            gOption->arrowY = 0;
            break;
        case 7:
            ret = 7;
            gOption->markX = 0xB4;
            gOption->markY = 0;
            gOption->arrowX = 0;
            gOption->arrowY = -0x28;
            break;
    }
    return ret;
}

/* The "reset to defaults?" dialog of the three setting pages and of the screen adjustment. */
void Option_UpdateReset(void) {
    s32 answer;
    s32 i;

    switch (gOption->resetStep) {
        case 0:
            Dialog_Init(MPACK_AT(gOption->res, 35), NULL, 0);
            Dialog_SetCursor(1);
            Dialog_SetMsgTable(gOption->dialogMsg);
            Dialog_SetMsg(0);
            Dialog_SetChoices(1);
            Dialog_Start(0);
            gOption->resetStep = 1;
            break;
        case 1:
            answer = Dialog_Input(1);
            if (answer < 0) {
                gOption->resetStep = 3;
            } else if (answer > 0) {
                gOption->resetStep = 2;
            }
            break;
        case 2:
            if (gOption->state == YOPT_SCR_RESET) {
                if (gSaveData->screenX != 0 || gSaveData->screenY != 0 || gSaveData->unk1694 != 0 ||
                    gSaveData->unk1698 != 0) {
                    gOption->dirty = 1;
                }
                gSaveData->screenX = gSaveData->screenY = 0;
                gSaveData->unk1694 = 0;
                gSaveData->unk1698 = 0;
            } else if (gOption->state == YOPT_ADJUST_RESET) {
                if (gSaveData->screenX != 0 || gSaveData->screenY != 0) {
                    gOption->dirty = 1;
                }
                gSaveData->screenX = gSaveData->screenY = 0;
            } else if (gOption->state == YOPT_SND_RESET) {
                if (gSaveData->soundMode != 0 || gSaveData->bgmVolume != 9 || gSaveData->seVolume != 9 ||
                    !(gSaveData->flags & SAVE_FLAG_VOICE)) {
                    gOption->dirty = 1;
                }
                gSaveData->soundMode = 0;
                gSaveData->bgmVolume = 9;
                gSaveData->seVolume = 9;
                gSaveData->flags |= SAVE_FLAG_VOICE;
                SndOpt_Apply();
            } else if (gOption->state == YOPT_CTRL_RESET) {
                if (gSaveData->flags & 8) {
                    gSaveData->flags &= ~8;
                }
                if (gSaveData->flags & 0x10) {
                    gSaveData->flags &= ~0x10;
                }
                if (!(gSaveData->flags & 2)) {
                    gSaveData->flags |= 2;
                }
                if (!(gSaveData->flags & 4)) {
                    gSaveData->flags |= 4;
                }
                for (i = 0; i < 2; i++) {
                    gSaveData->keyEdit[i][0] = 2;
                    gSaveData->keyEdit[i][1] = 1;
                    gSaveData->keyEdit[i][2] = 0;
                    gSaveData->keyEdit[i][3] = 3;
                    gSaveData->keyEdit[i][4] = 4;
                    gSaveData->keyEdit[i][5] = 5;
                    gSaveData->keyEdit[i][6] = 6;
                    gSaveData->keyEdit[i][7] = 7;
                }
            }
            gOption->resetStep = 3;
            break;
        case 3:
            Dialog_SetChoices(0);
            Dialog_Start(1);
            gOption->resetStep = 4;
            break;
        case 4:
            if (Dialog_IsClosed()) {
                if (gOption->state == YOPT_SCR_RESET) {
                    gOption->state = YOPT_SCREEN;
                } else if (gOption->state == YOPT_ADJUST_RESET) {
                    gOption->state = YOPT_ADJUST;
                } else if (gOption->state == YOPT_SND_RESET) {
                    gOption->state = YOPT_SOUND;
                } else {
                    gOption->state = YOPT_CTRL;
                }
                if (gOption->state != YOPT_ADJUST) {
                    Option_PlateGoto(0, OPT_CLIP_BOTTOM, OPT_STR_ON_START);
                }
                gOption->resetStep = 0;
            }
            break;
    }
}

/* Gives the two text clips of a bottom plate their rectangle and their first picture. */
void Option_SetBottomText(MFlash *flash, MFlashRef *ref, char *name, MFlashUv uv) {
    Flash_FindLabel(flash, name, "mc_bottom_text_off", ref);
    Flash_ClipSetUv(flash, ref, &uv);
    Flash_ClipSetTex(flash, ref, 0);
    Flash_FindLabel(flash, name, "mc_bottom_text_on", ref);
    Flash_ClipSetUv(flash, ref, &uv);
    Flash_ClipSetTex(flash, ref, 0);
}

/* The same for a menu plate. */
void Option_SetMenuText(MFlash *flash, MFlashRef *ref, char *name, MFlashUv uv) {
    Flash_FindLabel(flash, name, "mc_menu_text_off", ref);
    Flash_ClipSetUv(flash, ref, &uv);
    Flash_ClipSetTex(flash, ref, 0);
    Flash_FindLabel(flash, name, "mc_menu_text_on", ref);
    Flash_ClipSetUv(flash, ref, &uv);
    Flash_ClipSetTex(flash, ref, 0);
}

/* Dims the four picker plates. */
void Option_DimPickers(void) {
    MFlashRef ref;
    char name[0x40];
    MFlash *flash = gOption->flash;

    sprintf(name, "mc_select_plate_%d", 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, OPT_STR_OFF_START);
    sprintf(name, "mc_select_plate_%d", 2);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, OPT_STR_OFF_START);
    sprintf(name, "mc_select_plate_%d", 3);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, OPT_STR_OFF_START);
    sprintf(name, "mc_select_plate_%d", 4);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, OPT_STR_OFF_START);
}

/* Clip callback: limits drawing to the sound test's list. */
void Option_SetBgmScissor(void) {
    Sprite_SetScissor(0, 0x200, 0x45, 0x174);
}

/* Clip callback: back to the whole screen. */
void Option_ResetScissor(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* McFlow "done" callback: the settings are saved. */
void Option_OnSaved(void) {
    gOption->unk194 = 0;
    gOption->dirty = 0;
}

/* Frees the screen. */
void Option_Term(void) {
    s32 i;

    Dialog_Term();
    MsgWin_Term();
    IconWin_Term();
    McFlow_Term();
    for (i = 0; i < OPTION_FLASH_NUM; i++) {
        Flash_Destroy(&gOption->flash[i]);
    }
    if (gOption->res != NULL) {
        Heap_Free(gOption->res);
        gOption->res = NULL;
    }
    if (gOption != NULL) {
        Heap_Free(gOption);
        gOption = NULL;
    }
}
