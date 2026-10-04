#include "common.h"
#include "menu/menu_v.h"

/*
 * Menu overlay DBZP.BIN, 0x399790..0x39A978: head of the Shop object (the item shop of modes 48..50; the
 * object continues in the next chunk, which holds its draw / update / input / run functions).
 */

/* Raises the shop's stock level (once per level) when all collectable items but one are owned. */
void Shop_CheckStockLevel(void) {
    s32 total = 0;
    s32 owned = 0;
    s32 i;
    s32 flags;

    if (!(gSaveData->unk1008 & 2)) {
        for (i = 0; i < VLIST_ITEM_MAX; i++) {
            flags = gShop->items[i].flags;
            if (flags & VITEM_HIDDEN) {
                continue;
            }
            if (!(flags & VITEM_LISTED)) {
                continue;
            }
            if (flags & VITEM_UNCOUNTED) {
                continue;
            }
            total++;
            owned += gSaveData->item[i] & 1;
        }
        if (owned == total - 1) {
            gSaveData->unk100C++;
            if ((u32)gSaveData->unk100C >= 6) {
                gSaveData->unk100C = 5;
            }
            gSaveData->unk1008 |= 1;
            gSaveData->unk1008 |= 2;
        }
    }
}

/* Whether every item the shop sells is owned. */
s32 Shop_IsSoldOut(void) {
    s32 ret = 1;
    s32 i;

    for (i = 0; i < gShop->list[0].count[0]; i++) {
        if (!(u8)(gSaveData->item[gShop->list[0].ids[0][i]] & 1)) {
            ret = 0;
            break;
        }
    }
    return ret;
}

/* Whether an item can be bought: not owned, not hidden and sold by the shop. */
s32 Shop_CanBuy(s32 item, VItemEntry *table) {
    s32 ret = 0;

    if (!(u8)(gSaveData->item[item] & 1)) {
        if (!(table[item].flags & VITEM_HIDDEN)) {
            if (table[item].flags & VITEM_SOLD) {
                ret = 1;
            }
        }
    }
    return ret;
}

/* Starts the current guide's line for the current page. */
void Shop_PlayVoice(void) {
    switch (gShop->page) {
    case 0:
        gShop->voiceLine = gShop->guide * 15 + 1;
        break;
    case 1:
        gShop->voiceLine = gShop->guide * 15 + 3;
        break;
    }
    Voice_PlayWithSubtitle(gShop->subtitles, SHOP_VOICE_BASE, gShop->voiceLine);
}

/* Clips drawing to the item list of the current page. */
void Shop_SetListScissor(void) {
    switch (gShop->page) {
    case 0:
        Sprite_SetScissor(0, 0x200, 0x86, 0x116);
        break;
    case 1:
        Sprite_SetScissor(0, 0x200, 0x86, 0x17F);
        break;
    }
}

/* Clips drawing to the window in the middle of the screen. */
void Shop_SetWindowScissor(void) {
    Sprite_SetScissor(0xA4, 0x1C1, 0x3D, 0x12D);
}

/* Clips drawing to the whole screen again. */
void Shop_ResetScissor(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* Switches to the other guide and points the guide movie's textures at it. */
void Shop_SwapGuide(void) {
    MTexRes *res;

    gShop->guide ^= 1;
    res = gShop->guideRes[gShop->guide];
    switch (gShop->guide) {
    case 0:
        gShop->tex3[0] = MTEX(res, 0);
        gShop->tex3[3] = MTEX(res, 1);
        gShop->tex3[4] = MTEX(res, 3);
        gShop->tex3[1] = NULL;
        gShop->tex3[2] = NULL;
        break;
    case 1:
        gShop->tex3[0] = MTEX(res, 0);
        gShop->tex3[1] = MTEX(res, 1);
        gShop->tex3[2] = MTEX(res, 3);
        gShop->tex3[3] = NULL;
        gShop->tex3[4] = NULL;
        break;
    }
}

#define SHOP_RES(n) \
    res = (MTexRes *)MPACK_AT(gShop->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Allocates the shop, unpacks its section of archive 7, builds the six movies and the two item lists. */
void Shop_Init(s32 section) {
    MTexRes *res = NULL;
    s32 k = 0;
    s32 i;
    s32 flags;

    gShop = Heap_Alloc(sizeof(Shop), 0x20, 0, 2);
    memset(gShop, 0, sizeof(Shop));
    gShop->pack = MPACK_AT(gMenuArc7, section);
    gShop->res = Sprite_Unpack(gShop->pack, NULL, NULL);

    SHOP_RES(1);
    gShop->bg = res;

    SHOP_RES(31);
    gShop->tex0[0] = MTEX(res, 0);
    gShop->tex0[2] = MTEX(res, 1);
    gShop->tex0[1] = MTEX(res, 2);
    gShop->tex0[3] = MTEX(res, 3);
    SHOP_RES(2);
    gShop->tex0[4] = MTEX(res, 0);
    gShop->tex0[7] = MTEX(res, 1);
    gShop->tex0[6] = MTEX(res, 2);
    SHOP_RES(3);
    gShop->tex0[5] = MTEX(res, 0);
    gShop->tex0[8] = MTEX(res, 1);
    SHOP_RES(4);
    gShop->tex0[9] = MTEX(res, 0);
    gShop->tex0[13] = MTEX(res, 1);
    gShop->tex0[14] = MTEX(res, 2);
    SHOP_RES(5);
    gShop->tex0[10] = MTEX(res, 0);
    gShop->tex0[12] = MTEX(res, 1);
    gShop->tex0[11] = MTEX(res, 2);
    Flash_Create(&gShop->flash[0], MPACK_AT(gShop->res, 6), gShop->tex0);
    Flash_Play(&gShop->flash[0], 1);

    SHOP_RES(7);
    gShop->guideRes[0] = res;
    gShop->tex3[0] = MTEX(res, 0);
    gShop->tex3[3] = MTEX(res, 1);
    gShop->tex3[4] = MTEX(res, 3);
    SHOP_RES(8);
    gShop->guideRes[1] = res;
    Flash_Create(&gShop->flash[3], MPACK_AT(gShop->res, 9), gShop->tex3);
    Flash_Play(&gShop->flash[3], 1);

    SHOP_RES(10);
    gShop->tex1[1] = MTEX(res, 0);
    SHOP_RES(11);
    gShop->tex1[2] = MTEX(res, 0);
    gShop->tex1[3] = MTEX(res, 1);
    gShop->tex1[4] = MTEX(res, 2);
    gShop->tex2[14] = MTEX(res, 0);
    gShop->tex2[15] = MTEX(res, 1);
    gShop->tex2[16] = MTEX(res, 2);
    SHOP_RES(12);
    gShop->tex1[5] = MTEX(res, 0);
    gShop->tex1[6] = MTEX(res, 1);
    gShop->tex1[18] = MTEX(res, 2);
    SHOP_RES(13);
    gShop->tex1[7] = MTEX(res, 0);
    gShop->tex1[10] = MTEX(res, 1);
    gShop->tex1[8] = MTEX(res, 2);
    gShop->tex1[11] = MTEX(res, 3);
    gShop->tex1[0] = MTEX(res, 4);
    gShop->tex2[3] = MTEX(res, 0);
    gShop->tex2[6] = MTEX(res, 1);
    gShop->tex2[4] = MTEX(res, 2);
    gShop->tex2[7] = MTEX(res, 3);
    gShop->tex2[0] = MTEX(res, 4);
    SHOP_RES(14);
    gShop->tex1[12] = MTEX(res, 0);
    gShop->tex1[9] = MTEX(res, 1);
    gShop->tex1[19] = MTEX(res, 2);
    gShop->tex2[8] = MTEX(res, 0);
    gShop->tex2[5] = MTEX(res, 1);
    gShop->tex5[0] = MTEX(res, 3);
    SHOP_RES(15);
    gShop->tex1[17] = MTEX(res, 0);
    gShop->tex1[16] = MTEX(res, 1);
    gShop->tex2[13] = MTEX(res, 0);
    gShop->tex2[12] = MTEX(res, 1);
    SHOP_RES(16);
    gShop->tex1[13] = MTEX(res, 0);
    gShop->tex1[14] = MTEX(res, 0);
    gShop->tex2[9] = MTEX(res, 0);
    gShop->tex2[10] = MTEX(res, 0);
    SHOP_RES(24);
    gShop->tex1[20] = MTEX(res, 1);
    gShop->tex2[17] = MTEX(res, 1);
    Flash_Create(&gShop->flash[1], MPACK_AT(gShop->res, 17), gShop->tex1);
    Flash_Play(&gShop->flash[1], 1);

    SHOP_RES(18);
    gShop->tex2[2] = MTEX(res, 0);
    gShop->tex2[1] = MTEX(res, 1);
    Flash_Create(&gShop->flash[2], MPACK_AT(gShop->res, 19), gShop->tex2);
    Flash_Play(&gShop->flash[2], 1);

    SHOP_RES(27);
    gShop->tex4[0] = MTEX(res, 0);
    gShop->tex4[2] = MTEX(res, 1);
    gShop->tex4[4] = MTEX(res, 2);
    gShop->tex4[1] = MTEX(res, 3);
    SHOP_RES(28);
    gShop->tex4[3] = MTEX(res, 0);
    gShop->tex4[5] = MTEX(res, 1);
    Flash_Create(&gShop->flash[4], MPACK_AT(gShop->res, 29), gShop->tex4);
    Flash_Play(&gShop->flash[4], 1);

    Flash_Create(&gShop->flash[5], MPACK_AT(gShop->res, 33), gShop->tex5);
    Flash_Play(&gShop->flash[5], 1);

    SHOP_RES(20);
    IconWin_Init(MPACK_AT(gShop->res, 22), res);
    IconWin_Open();

    gShop->msgText = MPACK_AT(gShop->res, 32);
    gShop->subtitles = MPACK_AT(gShop->res, 26);
    MsgWin_Init(MPACK_AT(gShop->res, 21), gShop->msgText, 1, 0);
    MsgWin_Open();

    ItemHelp_Init((u32 *)MPACK_AT(gShop->res, 25));

    gShop->itemText = MPACK_AT(gShop->res, 30);
    gShop->items = (VItemEntry *)MPACK_AT(gCommonRes->data[2], 2);

    Shop_CheckStockLevel();

    for (; k < 2; k++) {
        for (i = 0; i < VLIST_ITEM_MAX; i++) {
            flags = gShop->items[i].flags;
            if (flags & VITEM_HIDDEN) {
                continue;
            }
            if (gShop->items[i].type >= 3) {
                continue;
            }
            if (!(flags & VITEM_LISTED)) {
                continue;
            }
            if (k == 0) {
                if (!(flags & VITEM_SOLD)) {
                    continue;
                }
                if ((u32)gSaveData->unk100C < gShop->items[i].stockLevel) {
                    continue;
                }
            }
            gShop->list[k].ids[0][gShop->list[k].count[0]] = i;
            gShop->list[k].count[0]++;
            gShop->list[k].ids[gShop->items[i].type + 1][gShop->list[k].count[gShop->items[i].type + 1]] = i;
            gShop->list[k].count[gShop->items[i].type + 1]++;
            switch (k) {
            case 0:
                break;
            case 1:
                if (!(gShop->items[i].flags & VITEM_UNCOUNTED)) {
                    gShop->total++;
                    if ((u8)(gSaveData->item[i] & 1)) {
                        gShop->owned++;
                    }
                }
                break;
            }
        }
        gShop->list[k].rows = k != 0 ? 7 : 4;
        gShop->list[k].rowsF = k != 0 ? 7.28f : 4.0f;
        for (i = 0; i < VLIST_TABS; i++) {
            gShop->list[k].cur[i] = 0;
            gShop->list[k].top[i] = gShop->list[k].cur[i];
            gShop->list[k].bottom[i] = gShop->list[k].cur[i] + gShop->list[k].rows - 1;
        }
    }

    gShop->percent = (f32)gShop->owned / (f32)gShop->total * 100.0f;
    if (gShop->owned != 0 && gShop->percent == 0) {
        gShop->percent = 1;
    }

    gShop->guide = Rand_Range(2);
    Shop_SwapGuide();
    gShop->voiceLine = -1;

    for (k = 0; k < SHOP_BOX_NUM; k++) {
        TextBox_Init(&gShop->box[k], gShop->itemText, 6);
        TextBox_SetUnk80(&gShop->box[k], 1);
        if (k < 8) {
            TextBox_SetRect(&gShop->box[k], 0, 0x200, 0x86, 0x17F);
        } else if (k < 13) {
            TextBox_SetRect(&gShop->box[k], 0, 0x200, 0x86, 0x116);
        }
    }
}
