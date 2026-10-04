#include "common.h"
#include "menu/menu_w.h"

/*
 * Menu overlay DBZP.BIN, 0x39A978..0x39E940: tail of the Shop object (the item shop of Evolution Z, progress
 * mode 50). The head of the object (Shop_CheckStockLevel .. Shop_Init, 0x399790..0x39A978) is
 * src/menu/menu_v_d.c: one source file, to be merged.
 */

/* The list a level drives. */
#define L0 gShop->list[0]
#define L1 gShop->list[1]

/* The item under row `i` of list `k` (0-based item number). */
#define SHOP_ROW_ITEM(k, i) \
    gShop->list[k].ids[gShop->list[k].tab][gShop->list[k].top[gShop->list[k].tab] + (i)]
/* The item on the extra plate of list `k`. */
#define SHOP_EXTRA_ITEM(k) gShop->list[k].ids[gShop->list[k].tab][gShop->list[k].extra]
/* The item under the cursor of list `k`. */
#define SHOP_CUR_ITEM(k) gShop->list[k].ids[gShop->list[k].tab][gShop->list[k].cur[gShop->list[k].tab]]

/* Starts line `n` of the current guide. */
#define SHOP_SAY(n) \
    gShop->voiceLine = gShop->guide * SHOP_GUIDE_LINES + (n); \
    Voice_PlayWithSubtitle(gShop->subtitles, SHOP_VOICE_BASE, gShop->voiceLine)

/* Frees the shop. */
void Shop_Term(void) {
    s32 i;

    ItemHelp_Term();
    MsgWin_Term();
    IconWin_Term();
    for (i = 0; i < SHOP_FLASH_NUM; i++) {
        Flash_Destroy(&gShop->flash[i]);
    }
    if (gShop->res != NULL) {
        Heap_Free(gShop->res);
        gShop->res = NULL;
    }
    if (gShop != NULL) {
        Heap_Free(gShop);
        gShop = NULL;
    }
}

/* The cost icon of an item: column by slot count, row by class. */
#define SHOP_COST_UV(item) \
    uv.x0 = gShop->items[item].slots * 0x20 - 0x20; \
    uv.y0 = ItemTbl_GetClass(item, gShop->items) * 0x2A; \
    uv.x1 = uv.x0 + 0x20; \
    uv.y1 = uv.y0 + 0x2A; \
    Flash_ClipSetUv(flash, &ref, &uv); \
    Flash_ClipSetFlags(flash, &ref, 2, 1)

/* The kind icon of an item. */
#define SHOP_KIND_UV(item) \
    uv.x0 = gShop->items[item].type * 0x40; \
    uv.y0 = 0; \
    uv.x1 = uv.x0 + 0x40; \
    uv.y1 = 0x40

/* Draws the backdrop, the six movies with everything that depends on the state, the windows and the details page. */
void Shop_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlash *flash;
    s32 i;
    s32 k;
    s32 count;
    s32 rows;
    s32 pos;
    f32 scale;

    Sprite_DrawPicture(gShop->bg, 0, 0, 0x80);

    flash = &gShop->flash[SHOP_FL_MAIN];
    for (i = 0; i < 2; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x200;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "menu_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    Num_Draw(flash, "mc_pay_num_%d", 0, 7, gSaveData->money, 0x20, 0x20, 0);
    Num_Draw(flash, "mc_item_num_%d", 0, 3, gShop->percent, 0x20, 0x20, 0);

    Flash_FindLabel(flash, NULL, "mc_bg_cloud", &ref);
    Flash_ClipSetCallbackA(flash, &ref, Shop_SetWindowScissor, NULL);
    Flash_ClipSetCallbackB(flash, &ref, Shop_ResetScissor, NULL);
    uv.x0 = 0;
    uv.y0 = 0;
    uv.x1 = 0x200;
    uv.y1 = 0x80;
    FlashAnim_Scroll(flash, &ref, &uv, &gShop->cloud, NULL, -0.14222222f, 0.0f);
    Flash_FindLabel(flash, NULL, "mc_ch_uron", &ref);
    Flash_ClipSetCallbackA(flash, &ref, Shop_SetWindowScissor, NULL);
    Flash_ClipSetCallbackB(flash, &ref, Shop_ResetScissor, NULL);

    flash = &gShop->flash[SHOP_FL_GUIDE];
    Flash_FindLabel(flash, "mc_guide_ranchi", gShop->guide != 0 ? "mc_guide_b_rnh_eye" : "mc_guide_rnh_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gShop->blink, 0);
    Flash_FindLabel(flash, "mc_guide_ranchi", gShop->guide != 0 ? "mc_guide_b_rnh_mouth" : "mc_guide_a_rnh_mouth",
                    &ref);
    FlashAnim_Talk(flash, &ref, &gShop->talk, 0);

    for (k = 0; k < 2; k++) {
        if (!(gShop->flags & (SHOP_LIST_BUY << k))) {
            continue;
        }
        flash = k != 0 ? &gShop->flash[SHOP_FL_ALL] : &gShop->flash[SHOP_FL_BUY];

        for (i = 0; i < SHOP_TABS; i++) {
            uv.x0 = i * 0x40;
            uv.y0 = 0;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = 0x40;
            sprintf(name, "mc_tab_btn_%d", i);
            Flash_FindLabel(flash, name, "mc_icon_tab_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_icon_tab_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_font_new_tab", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        uv.x0 = 0;
        uv.y0 = gShop->list[k].tab * 0x20;
        uv.x1 = 0x100;
        uv.y1 = uv.y0 + 0x20;
        Flash_FindLabel(flash, NULL, "mc_tab_text", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);

        if (k == 0) {
            /* plate 6: the item that was just bought */
            Flash_FindLabel(flash, "mc_list_plate_6", "mc_list_plate_off", &ref);
            Flash_ClipSetTex(flash, &ref, 2);
            SHOP_KIND_UV(gShop->item);
            Flash_FindLabel(flash, "mc_list_plate_6", "mc_icon_item_potara", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, "mc_list_plate_6", "mc_icon_item_cost", &ref);
            if (gShop->items[gShop->item].slots != 0) {
                SHOP_COST_UV(gShop->item);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            Flash_FindLabel(flash, "mc_list_plate_6", "mc_font_new", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, "mc_list_plate_6", "mc_dammy_text", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, gShop->item, &gShop->box[13]);
        }

        for (i = 0; i < gShop->list[k].rows; i++) {
            sprintf(name, "mc_list_plate_%d", i + 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 0x400, k != 0);
            if (i == 0 || i == gShop->list[k].rows - 1) {
                Flash_ClipSetCallbackA(flash, &ref, Shop_SetListScissor, NULL);
                Flash_ClipSetCallbackB(flash, &ref, Shop_ResetScissor, NULL);
            }
            switch (k) {
            case 0:
                if (gShop->list[k].top[gShop->list[k].tab] + i >= gShop->list[k].count[gShop->list[k].tab]) {
                    Flash_ClipSetColor(flash, &ref, 1.0f);
                    Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
                    Flash_ClipSetTex(flash, &ref, 7);
                    Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
                    Flash_ClipSetFlags(flash, &ref, 2, 0);
                    Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
                    TextBox_AttachLine(flash, &ref, 0, 0, -1, &gShop->box[8 + i]);
                    Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
                    Flash_ClipSetFlags(flash, &ref, 2, 0);
                } else {
                    if (Shop_CanBuy(SHOP_ROW_ITEM(k, i), gShop->items)) {
                        Flash_ClipSetColor(flash, &ref, 1.0f);
                    } else {
                        Flash_ClipSetColor(flash, &ref, 0.4f);
                    }
                    Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
                    Flash_ClipSetTex(flash, &ref, 2);
                    Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
                    if (gShop->items[SHOP_ROW_ITEM(k, i)].slots != 0) {
                        SHOP_COST_UV(SHOP_ROW_ITEM(k, i));
                    } else {
                        Flash_ClipSetFlags(flash, &ref, 2, 0);
                    }
                    Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
                    TextBox_AttachLine(flash, &ref, 0, 0, SHOP_ROW_ITEM(k, i), &gShop->box[8 + i]);
                    SHOP_KIND_UV(SHOP_ROW_ITEM(k, i));
                    Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
                    Flash_ClipSetUv(flash, &ref, &uv);
                    Flash_ClipSetFlags(flash, &ref, 2, 1);
                }
                break;
            case 1:
                Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
                if ((u8)(gSaveData->item[SHOP_ROW_ITEM(k, i)] & SAVE_ITEM_OWNED)) {
                    Flash_ClipSetTex(flash, &ref, 2);
                    Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
                    TextBox_AttachLine(flash, &ref, 0, 0, SHOP_ROW_ITEM(k, i), &gShop->box[i]);
                } else {
                    Flash_ClipSetTex(flash, &ref, 7);
                    Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
                    TextBox_AttachLine(flash, &ref, 0, 0, SHOP_ITEM_MAX, &gShop->box[i]);
                }
                Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
                if (gShop->items[SHOP_ROW_ITEM(k, i)].slots != 0 &&
                    (u8)(gSaveData->item[SHOP_ROW_ITEM(k, i)] & SAVE_ITEM_OWNED)) {
                    SHOP_COST_UV(SHOP_ROW_ITEM(k, i));
                } else {
                    Flash_ClipSetFlags(flash, &ref, 2, 0);
                }
                SHOP_KIND_UV(SHOP_ROW_ITEM(k, i));
                Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                break;
            }
            Flash_FindLabel(flash, name, "mc_font_new", &ref);
            if (k != 0) {
                if (gSaveData->item[SHOP_ROW_ITEM(k, i)] & SAVE_ITEM_NEW) {
                    Flash_ClipSetFlags(flash, &ref, 2, 1);
                } else {
                    Flash_ClipSetFlags(flash, &ref, 2, 0);
                }
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }

        /* the extra plate, visible while the list scrolls by one row */
        sprintf(name, "mc_list_plate_%d", gShop->list[k].rows + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetFlags(flash, &ref, 0x400, k != 0);
        Flash_ClipSetCallbackA(flash, &ref, Shop_SetListScissor, NULL);
        Flash_ClipSetCallbackB(flash, &ref, Shop_ResetScissor, NULL);
        switch (k) {
        case 0:
            if (gShop->list[k].extra >= gShop->list[k].count[gShop->list[k].tab]) {
                Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
                Flash_ClipSetTex(flash, &ref, 7);
            } else {
                if (Shop_CanBuy(SHOP_EXTRA_ITEM(k), gShop->items)) {
                    Flash_ClipSetColor(flash, &ref, 1.0f);
                } else {
                    Flash_ClipSetColor(flash, &ref, 0.4f);
                }
                Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
                Flash_ClipSetTex(flash, &ref, 2);
            }
            Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, SHOP_EXTRA_ITEM(k), &gShop->box[8 + gShop->list[k].rows]);
            Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
            if (gShop->items[SHOP_EXTRA_ITEM(k)].slots != 0) {
                SHOP_COST_UV(SHOP_EXTRA_ITEM(k));
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            break;
        case 1:
            Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
            if ((u8)(gSaveData->item[SHOP_EXTRA_ITEM(k)] & SAVE_ITEM_OWNED)) {
                Flash_ClipSetTex(flash, &ref, 2);
                Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
                TextBox_AttachLine(flash, &ref, 0, 0, SHOP_EXTRA_ITEM(k), &gShop->box[gShop->list[k].rows]);
            } else {
                Flash_ClipSetTex(flash, &ref, 7);
                Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
                TextBox_AttachLine(flash, &ref, 0, 0, SHOP_ITEM_MAX, &gShop->box[gShop->list[k].rows]);
            }
            Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
            if (gShop->items[SHOP_EXTRA_ITEM(k)].slots != 0 &&
                (u8)(gSaveData->item[SHOP_EXTRA_ITEM(k)] & SAVE_ITEM_OWNED)) {
                SHOP_COST_UV(SHOP_EXTRA_ITEM(k));
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            break;
        }
        SHOP_KIND_UV(SHOP_EXTRA_ITEM(k));
        Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_font_new", &ref);
        if (k != 0) {
            if (gSaveData->item[SHOP_EXTRA_ITEM(k)] & SAVE_ITEM_NEW) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }

        /* price of the item under the buy list's cursor */
        Num_Draw(flash, "mc_shop_num_%d", 0, 7, gShop->items[SHOP_CUR_ITEM(0)].price, 0x20, 0x20, 0);

        /* scroll arrows */
        for (i = 0; i < 2; i++) {
            uv.x0 = i * 0x20;
            uv.y0 = 0x20;
            uv.x1 = uv.x0 + 0x20;
            uv.y1 = 0x40;
            Flash_FindLabel(flash, "mc_menu_yajirusi", i != 0 ? "mc_menu_yajirusi_down" : "mc_menu_yajirusi_up", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if (i == 0) {
                Flash_ClipSetFlags(flash, &ref, 2, gShop->list[k].top[gShop->list[k].tab] != 0);
            } else if (gShop->list[k].count[gShop->list[k].tab] < gShop->list[k].rows ||
                       gShop->list[k].bottom[gShop->list[k].tab] == gShop->list[k].count[gShop->list[k].tab] - 1) {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            }
        }

        /* scroll bar */
        rows = gShop->list[k].rows;
        count = gShop->list[k].count[gShop->list[k].tab];
        if (count < rows) {
            count = rows;
        }
        pos = gShop->list[k].rowsF * 32.0f * gShop->list[k].top[gShop->list[k].tab] / count;
        scale = (f32)rows / count * gShop->list[k].rowsF;
        Flash_FindLabel(flash, NULL, "mc_scroll_bar_point", &ref);
        Flash_ClipSetScale(flash, &ref, 1.0f, scale);
        Flash_ClipSetOffset(flash, &ref, 0, pos);
    }

    flash = &gShop->flash[SHOP_FL_CONFIRM];
    for (i = 0; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x100;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_reconfir_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_reconfir_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_reconfir_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    IconWin_Draw();
    if (gShop->flags & SHOP_MSG_BEHIND) {
        MsgWin_Draw(0, 0, gShop->voiceLine);
        Flash_Draw(&gShop->flash[SHOP_FL_MAIN]);
        Flash_Draw(&gShop->flash[SHOP_FL_ALL]);
        Flash_Draw(&gShop->flash[SHOP_FL_BUY]);
        Flash_Draw(&gShop->flash[SHOP_FL_GUIDE]);
    } else {
        Flash_Draw(&gShop->flash[SHOP_FL_MAIN]);
        Flash_Draw(&gShop->flash[SHOP_FL_ALL]);
        Flash_Draw(&gShop->flash[SHOP_FL_BUY]);
        Flash_Draw(&gShop->flash[SHOP_FL_GUIDE]);
        MsgWin_Draw(0, 0, gShop->voiceLine);
    }
    Font_FlushAll();
    Flash_Draw(&gShop->flash[SHOP_FL_CONFIRM]);
    Flash_Draw(&gShop->flash[SHOP_FL_COMPLETE]);
    ItemHelp_Draw(gShop->helpItem);
}

/*
 * Plays `label` on the plate the cursor of a level is on: level 0 the menu plate, 1 / 2 the list row (or, with
 * `tab` set, the tab button) of the buy / collection list, 4 the row of the confirmation window.
 */
void Shop_SetPlate(s32 flash, s32 level, s32 tab, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *fl = &gShop->flash[flash];

    switch (level) {
    case SHOP_LV_MENU:
        sprintf(name, "menu_plate_%d", gShop->cursor[SHOP_LV_MENU] + 1);
        break;
    case SHOP_LV_BUY:
        if (tab != 0) {
            sprintf(name, "mc_tab_btn_%d", L0.tab);
        } else {
            sprintf(name, "mc_list_plate_%d", L0.cur[L0.tab] - L0.top[L0.tab] + 1);
        }
        break;
    case SHOP_LV_ALL:
        if (tab != 0) {
            sprintf(name, "mc_tab_btn_%d", L1.tab);
        } else {
            sprintf(name, "mc_list_plate_%d", L1.cur[L1.tab] - L1.top[L1.tab] + 1);
        }
        break;
    case SHOP_LV_CONFIRM:
        sprintf(name, "mc_reconfir_plate_%d", gShop->cursor[SHOP_LV_CONFIRM] + 1);
        break;
    }
    Flash_FindLabel(fl, NULL, name, &ref);
    Flash_ClipGotoLabel(fl, &ref, label);
}

/* Repeats the page's line while the guide is idle after the greeting, and advances the six movies. */
void Shop_Update(void) {
    s32 i;

    if (gShop->seq == 0 && (gShop->flags & SHOP_GREETED)) {
        if (gShop->voiceLine == gShop->guide * SHOP_GUIDE_LINES + SHOP_LINE_GREET ||
            gShop->voiceLine == gShop->guide * SHOP_GUIDE_LINES + SHOP_LINE_GREET_NEW) {
            if (Voice_GetStat(0) == MVOICE_IDLE) {
                Shop_PlayVoice();
            }
        }
    }
    for (i = 0; i < SHOP_FLASH_NUM; i++) {
        Flash_Advance(&gShop->flash[i]);
    }
}

/* Whether the pad asks for a page up / down: L1 / R1, or up / down with square held on a pad of status 1. */
#define SHOP_PAGE_UP \
    (gPad[0].status != 1 ? (gPad[0].gameRepeat & PADG_L1) \
                         : ((gPad[0].gameRepeat & PADG_UP) && (gPad[0].gameHeld & PADG_SQUARE)))
#define SHOP_PAGE_DOWN \
    (gPad[0].status != 1 ? (gPad[0].gameRepeat & PADG_R1) \
                         : ((gPad[0].gameRepeat & PADG_DOWN) && (gPad[0].gameHeld & PADG_SQUARE)))

/* Reads pad 0 for the current level. Leaving the shop writes 0 to *result. */
void Shop_Input(s32 *result) {
    if (!(gShop->flash[SHOP_FL_MAIN].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gShop->flags & SHOP_STARTED)) {
        Shop_SetPlate(0, 0, 0, "fl_on_start");
        gShop->flags |= SHOP_STARTED;
    }
    switch (gShop->level) {
    case SHOP_LV_MENU:
        if (gPad[0].gameRepeat & PADG_UP) {
            gShop->idle = 0;
            Shop_SetPlate(0, 0, 0, "fl_off_start");
            gShop->cursor[gShop->level]--;
            if (gShop->cursor[gShop->level] < 0) {
                gShop->cursor[gShop->level] = 1;
            }
            Shop_SetPlate(0, 0, 0, "fl_on_start");
            Shop_PlayVoice();
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            gShop->idle = 0;
            Shop_SetPlate(0, 0, 0, "fl_off_start");
            gShop->cursor[gShop->level]++;
            if (gShop->cursor[gShop->level] >= 2) {
                gShop->cursor[gShop->level] = 0;
            }
            Shop_SetPlate(0, 0, 0, "fl_on_start");
            Shop_PlayVoice();
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            gShop->idle = 0;
            Shop_SetPlate(0, 0, 0, "fl_ok");
            switch (gShop->cursor[gShop->level]) {
            case 0:
                if (gShop->percent == 100) {
                    SHOP_SAY(SHOP_LINE_ALL_OWNED);
                    Snd_PlaySe(1, 7);
                } else if (Shop_IsSoldOut()) {
                    SHOP_SAY(SHOP_LINE_SOLD_OUT);
                    Snd_PlaySe(1, 7);
                } else {
                    Flash_GotoLabel(&gShop->flash[SHOP_FL_MAIN], "fl_list_in", 1);
                    Flash_GotoLabel(&gShop->flash[SHOP_FL_BUY], "fl_list_in", 1);
                    Flash_GotoLabel(&gShop->flash[SHOP_FL_GUIDE], "fl_list_in", 1);
                    gShop->seq = 151;
                    Shop_SetPlate(1, 1, 0, "fl_on_start");
                    Shop_SetPlate(1, 1, 1, "fl_on_start");
                    gShop->flags |= SHOP_LIST_BUY;
                    gShop->flags &= ~SHOP_LIST_ALL;
                    SHOP_SAY(SHOP_LINE_LIST);
                    gShop->level = SHOP_LV_BUY;
                    Snd_PlaySe(1, 1);
                }
                break;
            case 1:
                Flash_GotoLabel(&gShop->flash[SHOP_FL_MAIN], "fl_list_in", 1);
                Flash_GotoLabel(&gShop->flash[SHOP_FL_ALL], "fl_list_in", 1);
                MsgWin_Close();
                Flash_GotoLabel(&gShop->flash[SHOP_FL_GUIDE], "fl_out", 1);
                gShop->flags |= SHOP_LIST_ALL;
                gShop->flags &= ~SHOP_LIST_BUY;
                Shop_SetPlate(2, 2, 0, "fl_on_start");
                Shop_SetPlate(2, 2, 1, "fl_on_start");
                gShop->level = SHOP_LV_ALL;
                Snd_PlaySe(1, 1);
                break;
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gShop->idle = 0;
            gShop->seq = 51;
            *result = 0;
            Snd_PlaySe(1, 2);
        } else {
            gShop->idle++;
            if (gShop->idle == 3600) {
                gShop->seq = 1;
                gShop->idle = 0;
            }
        }
        break;

    case SHOP_LV_BUY:
        if (SHOP_PAGE_UP) {
            if (L0.cur[L0.tab] > 0) {
                Shop_SetPlate(1, 1, 0, "fl_off_start");
                L0.cur[L0.tab] -= 4;
                if (L0.cur[L0.tab] < L0.top[L0.tab]) {
                    if (L0.cur[L0.tab] < 4) {
                        L0.extra = L0.top[L0.tab];
                        L0.cur[L0.tab] = 0;
                        L0.top[L0.tab] = 0;
                        L0.bottom[L0.tab] = 3;
                    } else {
                        /* original bug: reads the collection list's top row */
                        L0.extra = L1.top[L1.tab];
                        L0.top[L0.tab] -= 4;
                        L0.bottom[L0.tab] -= 4;
                    }
                }
                Shop_SetPlate(1, 1, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (SHOP_PAGE_DOWN) {
            if (L0.cur[L0.tab] < L0.count[L0.tab] - 1) {
                Shop_SetPlate(1, 1, 0, "fl_off_start");
                L0.cur[L0.tab] += 4;
                if (L0.cur[L0.tab] > L0.count[L0.tab] - 1) {
                    L0.cur[L0.tab] = L0.count[L0.tab] - 1;
                }
                if (L0.bottom[L0.tab] < L0.cur[L0.tab]) {
                    if (L0.cur[L0.tab] > L0.count[L0.tab] - 5) {
                        L0.cur[L0.tab] = L0.count[L0.tab] - 1;
                        L0.extra = L0.bottom[L0.tab];
                        L0.top[L0.tab] = L0.count[L0.tab] - 4;
                        L0.bottom[L0.tab] = L0.count[L0.tab] - 1;
                    } else {
                        L0.extra = L0.bottom[L0.tab];
                        L0.top[L0.tab] += 4;
                        L0.bottom[L0.tab] += 4;
                    }
                }
                Shop_SetPlate(1, 1, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & PADG_UP) {
            if (L0.cur[L0.tab] > 0) {
                Shop_SetPlate(1, 1, 0, "fl_off_start");
                L0.cur[L0.tab]--;
                if (L0.cur[L0.tab] < L0.top[L0.tab]) {
                    Flash_GotoLabel(&gShop->flash[SHOP_FL_BUY], "fl_list_down", 1);
                    L0.extra = L0.bottom[L0.tab];
                    L0.top[L0.tab]--;
                    L0.bottom[L0.tab]--;
                }
                Shop_SetPlate(1, 1, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (L0.cur[L0.tab] < L0.count[L0.tab] - 1) {
                Shop_SetPlate(1, 1, 0, "fl_off_start");
                L0.cur[L0.tab]++;
                if (L0.cur[L0.tab] > L0.bottom[L0.tab]) {
                    Flash_GotoLabel(&gShop->flash[SHOP_FL_BUY], "fl_list_up", 1);
                    L0.extra = L0.top[L0.tab];
                    L0.top[L0.tab]++;
                    L0.bottom[L0.tab]++;
                }
                Shop_SetPlate(1, 1, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & PADG_LEFT) {
            Shop_SetPlate(1, 1, 0, "fl_off_start");
            Shop_SetPlate(1, 1, 1, "fl_off_start");
            L0.tab--;
            if (L0.tab < 0) {
                L0.tab = SHOP_TABS - 1;
            }
            Shop_SetPlate(1, 1, 0, "fl_on_start");
            Shop_SetPlate(1, 1, 1, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & PADG_RIGHT) {
            Shop_SetPlate(1, 1, 0, "fl_off_start");
            Shop_SetPlate(1, 1, 1, "fl_off_start");
            L0.tab++;
            if (L0.tab >= SHOP_TABS) {
                L0.tab = 0;
            }
            Shop_SetPlate(1, 1, 0, "fl_on_start");
            Shop_SetPlate(1, 1, 1, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            Shop_SetPlate(1, 1, 0, "fl_ok");
            if (Shop_CanBuy(SHOP_CUR_ITEM(0), gShop->items)) {
                gShop->cursor[SHOP_LV_CONFIRM] = 1;
                Shop_SetPlate(4, 4, 0, "fl_on_start");
                Flash_GotoLabel(&gShop->flash[SHOP_FL_CONFIRM], "fl_window_in", 1);
                SHOP_SAY(SHOP_LINE_CONFIRM);
                gShop->level = SHOP_LV_CONFIRM;
                Snd_PlaySe(1, 1);
            } else {
                SHOP_SAY(SHOP_LINE_OWNED);
                Snd_PlaySe(1, 7);
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            Flash_GotoLabel(&gShop->flash[SHOP_FL_BUY], "fl_list_out", 1);
            Flash_GotoLabel(&gShop->flash[SHOP_FL_GUIDE], "fl_list_out", 1);
            Flash_GotoLabel(&gShop->flash[SHOP_FL_MAIN], "fl_list_out", 1);
            gShop->seq = 151;
            Shop_SetPlate(1, 1, 0, "fl_off_start");
            Shop_SetPlate(1, 1, 1, "fl_off_start");
            SHOP_SAY(SHOP_LINE_BACK);
            gShop->level = SHOP_LV_MENU;
            Snd_PlaySe(1, 2);
        }
        break;

    case SHOP_LV_ALL:
        if (SHOP_PAGE_UP) {
            if (L1.cur[L1.tab] > 0) {
                Shop_SetPlate(2, 2, 0, "fl_off_start");
                L1.cur[L1.tab] -= 7;
                if (L1.cur[L1.tab] < L1.top[L1.tab]) {
                    if (L1.cur[L1.tab] < 7) {
                        L1.extra = L1.top[L1.tab];
                        L1.cur[L1.tab] = 0;
                        L1.top[L1.tab] = 0;
                        L1.bottom[L1.tab] = 6;
                    } else {
                        L1.extra = L1.top[L1.tab];
                        L1.top[L1.tab] -= 7;
                        L1.bottom[L1.tab] -= 7;
                    }
                }
                Shop_SetPlate(2, 2, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (SHOP_PAGE_DOWN) {
            if (L1.cur[L1.tab] < L1.count[L1.tab] - 1) {
                Shop_SetPlate(2, 2, 0, "fl_off_start");
                L1.cur[L1.tab] += 7;
                if (L1.bottom[L1.tab] < L1.cur[L1.tab]) {
                    if (L1.cur[L1.tab] > L1.count[L1.tab] - 8) {
                        L1.cur[L1.tab] = L1.count[L1.tab] - 1;
                        L1.extra = L1.bottom[L1.tab];
                        L1.top[L1.tab] = L1.count[L1.tab] - 7;
                        L1.bottom[L1.tab] = L1.count[L1.tab] - 1;
                    } else {
                        L1.extra = L1.bottom[L1.tab];
                        L1.top[L1.tab] += 7;
                        L1.bottom[L1.tab] += 7;
                    }
                }
                Shop_SetPlate(2, 2, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & PADG_UP) {
            if (L1.cur[L1.tab] > 0) {
                Shop_SetPlate(2, 2, 0, "fl_off_start");
                L1.cur[L1.tab]--;
                if (L1.cur[L1.tab] < L1.top[L1.tab]) {
                    Flash_GotoLabel(&gShop->flash[SHOP_FL_ALL], "fl_list_down", 1);
                    L1.extra = L1.bottom[L1.tab];
                    L1.top[L1.tab]--;
                    L1.bottom[L1.tab]--;
                }
                Shop_SetPlate(2, 2, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (L1.cur[L1.tab] < L1.count[L1.tab] - 1) {
                Shop_SetPlate(2, 2, 0, "fl_off_start");
                L1.cur[L1.tab]++;
                if (L1.cur[L1.tab] > L1.bottom[L1.tab]) {
                    Flash_GotoLabel(&gShop->flash[SHOP_FL_ALL], "fl_list_up", 1);
                    L1.extra = L1.top[L1.tab];
                    L1.top[L1.tab]++;
                    L1.bottom[L1.tab]++;
                }
                Shop_SetPlate(2, 2, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & PADG_LEFT) {
            Shop_SetPlate(2, 2, 0, "fl_off_start");
            Shop_SetPlate(2, 2, 1, "fl_off_start");
            L1.tab--;
            if (L1.tab < 0) {
                L1.tab = SHOP_TABS - 1;
            }
            Shop_SetPlate(2, 2, 0, "fl_on_start");
            Shop_SetPlate(2, 2, 1, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & PADG_RIGHT) {
            Shop_SetPlate(2, 2, 0, "fl_off_start");
            Shop_SetPlate(2, 2, 1, "fl_off_start");
            L1.tab++;
            if (L1.tab >= SHOP_TABS) {
                L1.tab = 0;
            }
            Shop_SetPlate(2, 2, 0, "fl_on_start");
            Shop_SetPlate(2, 2, 1, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            Shop_SetPlate(2, 2, 0, "fl_ok");
            if ((u8)(gSaveData->item[SHOP_CUR_ITEM(1)] & SAVE_ITEM_OWNED)) {
                ItemHelp_Open();
                gShop->cursor[SHOP_LV_HELP] = gShop->level;
                gShop->helpItem = SHOP_CUR_ITEM(1);
                gSaveData->item[gShop->helpItem] &= ~SAVE_ITEM_NEW;
                gShop->level = SHOP_LV_HELP;
                Snd_PlaySe(1, 1);
            } else {
                Snd_PlaySe(1, 7);
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            Flash_GotoLabel(&gShop->flash[SHOP_FL_ALL], "fl_list_out", 1);
            MsgWin_Open();
            Flash_GotoLabel(&gShop->flash[SHOP_FL_GUIDE], "fl_in", 1);
            Flash_GotoLabel(&gShop->flash[SHOP_FL_MAIN], "fl_list_out", 1);
            Shop_SetPlate(2, 2, 0, "fl_off_start");
            Shop_SetPlate(2, 2, 1, "fl_off_start");
            SHOP_SAY(SHOP_LINE_BACK);
            gShop->level = SHOP_LV_MENU;
            Snd_PlaySe(1, 2);
        }
        break;

    case SHOP_LV_HELP:
        if (gPad[0].gamePressed & (PADG_CROSS | PADG_TRIANGLE)) {
            ItemHelp_Close();
            switch (gShop->cursor[SHOP_LV_HELP]) {
            case SHOP_LV_ALL:
                Shop_SetPlate(2, 2, 0, "fl_on_start");
                Shop_SetPlate(2, 2, 1, "fl_on_start");
                break;
            case SHOP_LV_CONFIRM:
                Shop_SetPlate(4, 4, 0, "fl_on_start");
                break;
            }
            gShop->level = gShop->cursor[SHOP_LV_HELP];
            Snd_PlaySe(1, 2);
        }
        break;

    case SHOP_LV_CONFIRM:
        if (gPad[0].gameRepeat & PADG_UP) {
            Shop_SetPlate(4, 4, 0, "fl_off_start");
            gShop->cursor[gShop->level]--;
            if (gShop->cursor[gShop->level] < 0) {
                gShop->cursor[gShop->level] = 2;
            }
            Shop_SetPlate(4, 4, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            Shop_SetPlate(4, 4, 0, "fl_off_start");
            gShop->cursor[gShop->level]++;
            if (gShop->cursor[gShop->level] >= 3) {
                gShop->cursor[gShop->level] = 0;
            }
            Shop_SetPlate(4, 4, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            switch (gShop->cursor[gShop->level]) {
            case 0:
                if ((u32)gSaveData->money < (u32)gShop->items[SHOP_CUR_ITEM(0)].price) {
                    SHOP_SAY(SHOP_LINE_NO_MONEY);
                    Snd_PlaySe(1, 7);
                } else {
                    Shop_SetPlate(4, 4, 0, "fl_ok");
                    Flash_GotoLabel(&gShop->flash[SHOP_FL_BUY], "fl_get_in", 1);
                    Flash_GotoLabel(&gShop->flash[SHOP_FL_CONFIRM], "fl_window_out", 1);
                    gShop->item = SHOP_CUR_ITEM(0);
                    Save_AddItem(gShop->item);
                    gSaveData->money -= gShop->items[gShop->item].price;
                    gShop->owned++;
                    gShop->percent = (f32)gShop->owned / (f32)gShop->total * 100.0f;
                    if (gShop->owned != 0 && gShop->percent == 0) {
                        gShop->percent = 1;
                    }
                    gProgress->flags |= 1;
                    gShop->level = SHOP_LV_BOUGHT;
                    Snd_PlaySe(1, 1);
                }
                break;
            case 1:
                Flash_GotoLabel(&gShop->flash[SHOP_FL_CONFIRM], "fl_window_out", 1);
                Shop_SetPlate(4, 4, 0, "fl_ok");
                gShop->level = SHOP_LV_BUY;
                Snd_PlaySe(1, 2);
                break;
            case 2:
                Shop_SetPlate(4, 4, 0, "fl_ok");
                ItemHelp_Open();
                gShop->cursor[SHOP_LV_HELP] = gShop->level;
                gShop->helpItem = SHOP_CUR_ITEM(0);
                gShop->level = SHOP_LV_HELP;
                Snd_PlaySe(1, 1);
                break;
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            Flash_GotoLabel(&gShop->flash[SHOP_FL_CONFIRM], "fl_window_out", 1);
            Shop_SetPlate(4, 4, 0, "fl_off_start");
            gShop->level = SHOP_LV_BUY;
            Snd_PlaySe(1, 2);
        }
        break;

    case SHOP_LV_BOUGHT:
        if (gPad[0].gamePressed & PADG_CROSS) {
            if (gShop->percent == 100) {
                gShop->seq = 101;
            } else {
                SHOP_SAY(SHOP_LINE_THANKS);
            }
            Flash_GotoLabel(&gShop->flash[SHOP_FL_BUY], "fl_get_out", 1);
            gShop->level = SHOP_LV_BUY;
            Snd_PlaySe(1, 1);
        }
        break;
    }
}

/*
 * One step of the scripted sequence: 1.. the guides swap after 3600 idle frames, 51.. the farewell and the fade
 * out, 101.. the "complete" caption at 100 %, 151.. the message window changes sides of the draw order while a
 * list slides in or out.
 */
void Shop_UpdateSeq(void) {
    if (gShop->seq == 0) {
        return;
    }
    switch (gShop->seq) {
    case 1:
        Flash_GotoLabel(&gShop->flash[SHOP_FL_GUIDE], "fl_out", 1);
        gShop->seq++;
        break;
    case 2:
        if (gShop->flash[SHOP_FL_GUIDE].trig & 1) {
            gShop->seq++;
        }
        break;
    case 3:
        SHOP_SAY(SHOP_LINE_SWAP);
        gShop->seq++;
        break;
    case 4:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            Snd_PlaySe(2, 0x23);
            Shop_SwapGuide();
            gShop->seq++;
        }
        break;
    case 5:
        Flash_GotoLabel(&gShop->flash[SHOP_FL_GUIDE], "fl_in", 1);
        gShop->seq++;
        break;
    case 6:
        if (gShop->flash[SHOP_FL_GUIDE].trig & 1) {
            SHOP_SAY(SHOP_LINE_GREET);
            gShop->seq++;
        }
        break;
    case 7:
        gShop->seq = 0;
        break;

    case 51:
        SHOP_SAY(SHOP_LINE_BYE);
        gShop->seq++;
        break;
    case 52:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gShop->seq++;
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            gShop->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gShop->seq++;
            Snd_PlaySe(1, 2);
        }
        break;
    case 53:
        ColorFade_StartOut(0, 0, 0, 0x14);
        Flash_GotoLabel(&gShop->flash[SHOP_FL_MAIN], "fl_out", 1);
        gShop->seq = 0;
        break;

    case 101:
        gShop->seq++;
        /* fallthrough */
    case 102:
        if (gShop->flash[SHOP_FL_BUY].trig & 1) {
            Flash_GotoLabel(&gShop->flash[SHOP_FL_COMPLETE], "fl_font_complete_in", 1);
            gShop->seq++;
        }
        break;
    case 103:
        if (gShop->flash[SHOP_FL_COMPLETE].trig & 2) {
            SHOP_SAY(SHOP_LINE_COMPLETE);
            gShop->seq++;
        }
        break;
    case 104:
        if (gPad[0].gamePressed & PADG_CROSS) {
            Flash_GotoLabel(&gShop->flash[SHOP_FL_COMPLETE], "fl_font_complete_out", 1);
            gShop->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 105:
        if (gShop->flash[SHOP_FL_COMPLETE].trig & 2) {
            gShop->seq++;
        }
        break;
    case 106:
        Flash_GotoLabel(&gShop->flash[SHOP_FL_BUY], "fl_list_out", 1);
        Flash_GotoLabel(&gShop->flash[SHOP_FL_GUIDE], "fl_list_out", 1);
        Flash_GotoLabel(&gShop->flash[SHOP_FL_MAIN], "fl_list_out", 1);
        Shop_SetPlate(1, 1, 0, "fl_off_start");
        Shop_SetPlate(1, 1, 1, "fl_off_start");
        gShop->level = SHOP_LV_MENU;
        gShop->seq = 0;
        break;

    case 151:
        gShop->seq++;
        /* fallthrough */
    case 152:
        if (gShop->flash[SHOP_FL_GUIDE].trig & 2) {
            gShop->seq++;
        }
        break;
    case 153:
        gShop->seq = 0;
        if (gShop->flags & SHOP_MSG_BEHIND) {
            gShop->flags ^= SHOP_MSG_BEHIND;
        } else {
            gShop->flags |= SHOP_MSG_BEHIND;
        }
        break;
    }
}

/* Runs the shop until its fade out is over. Returns 1, or 0 when the player left with the cancel button. */
s32 Shop_Run(s32 section) {
    s32 result = 1;

    Shop_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            Shop_Update();
            Shop_UpdateSeq();
        }
        Shop_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gShop->flags & SHOP_GREETED) && (gShop->flash[SHOP_FL_MAIN].flags & MFLASH_PAD)) {
                gShop->flags |= SHOP_GREETED;
                if (gSaveData->unk1008 & 1) {
                    gShop->voiceLine = gShop->guide * SHOP_GUIDE_LINES + SHOP_LINE_GREET_NEW;
                    gSaveData->unk1008 &= ~1;
                } else {
                    gShop->voiceLine = gShop->guide * SHOP_GUIDE_LINES + SHOP_LINE_GREET;
                }
                Voice_PlayWithSubtitle(gShop->subtitles, SHOP_VOICE_BASE, gShop->voiceLine);
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gShop->flags & SHOP_LEAVING) {
            if (--gShop->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gShop->seq == 0) {
            Shop_Input(&result);
        }
    }
    Shop_Term();
    Dma_ResetBuffers();
    return result;
}
