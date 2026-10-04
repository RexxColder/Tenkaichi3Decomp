#include "common.h"
#include "menu/menu_b.h"
#include "sys/save.h"

/*
 * HistSel, from 0x33CFC8 (this file ends at 0x33E108; the object goes on in the next chunk, which has the rest
 * of the screen and its frame loop func_0033F440): the saga select of the story mode (progress mode 6).
 */

extern void Voice_StopWithLip(void);

/* Says line voiceLine with the voice set of guide `guide` (0..7 the sagas' guides, 8 this screen's own). */
void HistSel_PlayVoice(void) {
    switch (gHistSel->guide) {
    case 0:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8537, gHistSel->voiceLine);
        break;
    case 1:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8423, gHistSel->voiceLine);
        break;
    case 2:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x83B4, gHistSel->voiceLine);
        break;
    case 3:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x83DB, gHistSel->voiceLine);
        break;
    case 4:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8484, gHistSel->voiceLine);
        break;
    case 5:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8440, gHistSel->voiceLine);
        break;
    case 6:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8402, gHistSel->voiceLine);
        break;
    case 7:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8466, gHistSel->voiceLine);
        break;
    case 8:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x855B, gHistSel->voiceLine);
        break;
    }
}

/* Counts the frames without input: after 3600 the screen's guide says one of four idle lines. */
void HistSel_Idle(void) {
    gHistSel->idle++;
    if (gHistSel->idle == 3600) {
        gHistSel->guide = 8;
        gHistSel->unk230 = 0;
        gHistSel->voiceLine = Rand_Range(4) + 10;
        HistSel_PlayVoice();
        gHistSel->idle = 0;
    }
}

/* The guide presents the saga under the cursor (one of two lines each); an empty plate stops the voice. */
void HistSel_SayItem(void) {
    switch (gHistSel->items[gHistSel->cursor]) {
    case 0:
        gHistSel->voiceLine = Rand_Range(2) + 0x16;
        HistSel_PlayVoice();
        break;
    case 1:
        gHistSel->voiceLine = Rand_Range(2) + 0x18;
        HistSel_PlayVoice();
        break;
    case 2:
        gHistSel->voiceLine = Rand_Range(2) + 0x1A;
        HistSel_PlayVoice();
        break;
    case 3:
        gHistSel->voiceLine = Rand_Range(2) + 0x1C;
        HistSel_PlayVoice();
        break;
    case 4:
        gHistSel->voiceLine = Rand_Range(2) + 0x20;
        HistSel_PlayVoice();
        break;
    case 5:
        gHistSel->voiceLine = Rand_Range(2) + 0x1E;
        HistSel_PlayVoice();
        break;
    case 6:
        gHistSel->voiceLine = Rand_Range(2) + 0x22;
        HistSel_PlayVoice();
        break;
    case 7:
        gHistSel->voiceLine = Rand_Range(2) + 0x24;
        HistSel_PlayVoice();
        break;
    case 8:
        gHistSel->voiceLine = 0x2F;
        HistSel_PlayVoice();
        break;
    default:
        gHistSel->voiceLine = -1;
        Voice_StopWithLip();
        break;
    }
}

/* Each saga whose outro was seen and that has not had its event (slot flag 0x20) gets a 4 % chance, in order;
   the first hit becomes this visit's event saga. */
void HistSel_RollEvent(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        SaveSlot *slot = &gSaveData->slot[i];
        s32 flags = slot->flags;
        s32 done = flags & SAVESLOT_EVENT_DONE;

        flags &= SAVESLOT_OUTRO_SEEN;
        if (flags && !done && (s32)Rand_Range(100) < 4) {
            gHistSel->event = i;
            return;
        }
    }
}

#define SEL_RES(n) \
    res = (MTexRes *)MPACK_AT(gHistSel->res, n); \
    Res_RelocateOffsets(&res, res, res)

#define SEL_TEX(n, k) gHistSel->tex[n] = MTEX(res, k)

#define SEL_ICONS(a, b, c) \
    SEL_TEX(a, 0); \
    SEL_TEX(b, 1); \
    SEL_TEX(c, 3)

/* Loads the screen (section `section` of archive 2), lists the unlocked sagas and works out the completion
   percentage of the story mode. */
void HistSel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    s32 j;
    s32 cleared;

    gHistSel = Heap_Alloc(0x254, 0x20, 0, 2);
    memset(gHistSel, 0, 0x254);
    gHistSel->pack = (u32 *)MPACK_AT(gMenuArc2, section);
    gHistSel->res = Sprite_Unpack(gHistSel->pack, NULL, NULL);
    StreamSe_PlayDefault(0, 0x10BDB);
    SEL_RES(1);
    gHistSel->bg = res;
    SEL_RES(2);
    SEL_TEX(30, 0);
    SEL_TEX(29, 1);
    SEL_TEX(22, 2);
    SEL_TEX(0, 3);
    SEL_RES(3);
    SEL_TEX(1, 0);
    SEL_TEX(2, 1);
    SEL_TEX(3, 2);
    SEL_TEX(4, 3);
    SEL_RES(4);
    SEL_TEX(23, 0);
    SEL_TEX(24, 1);
    SEL_TEX(25, 2);
    SEL_TEX(26, 3);
    SEL_TEX(27, 4);
    SEL_TEX(28, 5);
    SEL_RES(5);
    SEL_TEX(74, 0);
    SEL_TEX(75, 1);
    SEL_TEX(5, 2);
    SEL_TEX(71, 3);
    SEL_TEX(70, 4);
    SEL_TEX(73, 5);
    SEL_TEX(72, 6);
    for (i = 0; i < 16; i++) {
        gHistSel->tex[6 + i] = MTEX(res, i) + 7 * 0x40;
    }
    SEL_RES(6);
    SEL_ICONS(31, 32, 33);
    SEL_RES(7);
    SEL_TEX(36, 0);
    SEL_RES(8);
    SEL_TEX(44, 0);
    SEL_TEX(40, 1);
    SEL_TEX(43, 2);
    SEL_TEX(42, 3);
    SEL_TEX(37, 4);
    SEL_TEX(76, 5);
    SEL_TEX(79, 6);
    SEL_TEX(78, 7);
    SEL_TEX(81, 8);
    SEL_RES(9);
    SEL_TEX(39, 0);
    SEL_TEX(82, 1);
    SEL_TEX(34, 2);
    SEL_TEX(35, 3);
    SEL_TEX(41, 4);
    SEL_TEX(45, 5);
    SEL_TEX(38, 6);
    SEL_TEX(77, 8);
    for (i = 0; i < 8; i++) {
        SEL_RES(33 + i);
        switch (i) {
        case 0:
            SEL_ICONS(46, 47, 48);
            break;
        case 1:
            SEL_ICONS(49, 50, 51);
            break;
        case 2:
            SEL_ICONS(52, 53, 54);
            break;
        case 3:
            SEL_ICONS(55, 57, 56);
            break;
        case 4:
            SEL_ICONS(58, 59, 60);
            break;
        case 5:
            SEL_ICONS(61, 62, 63);
            break;
        case 6:
            SEL_ICONS(64, 65, 66);
            break;
        case 7:
            SEL_ICONS(67, 68, 69);
            break;
        }
    }
    SEL_RES(12);
    SEL_TEX(80, 1);
    Flash_Create(&gHistSel->flash[0], MPACK_AT(gHistSel->res, 10), gHistSel->tex);
    Flash_Play(&gHistSel->flash[0], 1);
    SEL_RES(11);
    IconWin_Init(MPACK_AT(gHistSel->res, 13), res);
    IconWin_Open();
    for (i = 0; i < 9; i++) {
        if (i == 8) {
            gHistSel->msgText[i] = MPACK_AT(gHistSel->res, 15);
            gHistSel->subtitles[i] = MPACK_AT(gHistSel->res, 24);
        } else {
            gHistSel->msgText[i] = MPACK_AT(gHistSel->res, 16 + i);
            gHistSel->subtitles[i] = MPACK_AT(gHistSel->res, 25 + i);
        }
    }
    MsgWin_Init(MPACK_AT(gHistSel->res, 14), NULL, 1, 0);
    MsgWin_Open();
    {
        s32 count[8] = { 3, 4, 7, 5, 16, 5, 4, 4 };

        gHistSel->itemCount = 0;
        cleared = 0;
        for (i = 0; i < 9; i++) {
            SaveSlot *slot = &gSaveData->slot[i];

            if (slot->flags & 1) {
                gHistSel->items[gHistSel->itemCount++] = i;
            }
            if (i < 8) {
                for (j = 0; j < count[i]; j++) {
                    if (gSaveData->slot[i].val[1] & (s32)(1U << j)) {
                        cleared++;
                    }
                }
            }
        }
    }
    for (i = gHistSel->itemCount; i < 3; i++) {
        gHistSel->items[i] = 9;
        gHistSel->itemCount++;
    }
    gHistSel->percent = (f32)cleared / 48.0f * 100.0f;
    if (cleared != 0 && gHistSel->percent == 0) {
        gHistSel->percent = 1;
    }
    if (gHistSel->percent == 100 && !(gSaveData->unlockFlags & 0x100)) {
        gHistSel->flags |= HISTSEL_COMPLETE;
    }
    for (i = 0; i < gHistSel->itemCount; i++) {
        if (gHistSel->items[i] == gProgress->subMenu) {
            gHistSel->cursor = i;
            break;
        }
    }
    gHistSel->top = gHistSel->cursor - 1;
    if (gHistSel->top < 0) {
        gHistSel->top += gHistSel->itemCount;
    }
    gHistSel->voiceLine = -1;
    gHistSel->guide = 8;
    gHistSel->event = -1;
    for (i = 0; i < 2; i++) {
        gHistSel->blink[i] = Rand_Range(0x20);
    }
    if (gSaveData->unlockFlags & 0x80) {
        HistSel_RollEvent();
        if (gHistSel->event >= 0) {
            gHistSel->flags |= HISTSEL_EVENT;
        }
    } else {
        gHistSel->flags |= HISTSEL_FIRST;
    }
    gHistSel->unk250 = Rand_Range(10) * 60 + 300;
}
