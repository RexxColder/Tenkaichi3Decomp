#include "common.h"
#include "menu/menu_b.h"
#include "sys/pad.h"
#include "sys/save.h"

/*
 * ModeMenu, 0x339610..0x33A360: the tail of the object that starts at 0x338020 (src/menu/menu_a_d.c). Its
 * strings are shared with the head ("mc_menu_plate_%d" is ModeMenu_Draw's), so the two files are one source.
 */

extern void Voice_StopWithLip(void);
extern void StreamSe_FadeOutStep(s32 se);
extern void BattleSetup_Clear(void);
extern void BattleSetup_SetScript(s32 script);

#define MODEMENU_ITEM(m) ((m)->items[(m)->cursor[0]])

/* Advances the movie, the greeting voice and the scroll of the description. */
void ModeMenu_Update(void) {
    s32 i;

    if ((gModeMenu->flags & MODEMENU_GREETED) &&
        (gModeMenu->voiceLine == gModeMenu->lineBase || gModeMenu->voiceLine == gModeMenu->lineBase + 3)) {
        if (Voice_GetStat(0) == 5) {
            gModeMenu->voiceLine = MODEMENU_ITEM(gModeMenu) + gModeMenu->lineBase + 4;
            Voice_PlayWithSubtitle(gModeMenu->subtitles, gModeMenu->voiceBase, gModeMenu->voiceLine);
        }
    }
    for (i = 0; i < MODEMENU_FLASH_NUM; i++) {
        Flash_Advance(&gModeMenu->flash[i]);
    }
    if (!(gModeMenu->flags & MODEMENU_STARTED) && (gModeMenu->flash[0].trig & 2)) {
        MsgWin_Open();
    }
    if (gModeMenu->flags & MODEMENU_SCROLLING) {
        gModeMenu->scroll += 0.23333333f;
        if (gModeMenu->scroll >= gModeMenu->scrollMax) {
            gModeMenu->scroll = gModeMenu->scrollMax;
            gModeMenu->flags &= ~MODEMENU_SCROLLING;
            gModeMenu->flags |= MODEMENU_NEXT;
        }
    }
}

/* Sends the plate under the cursor to a label of its clip (kind 0; other kinds do nothing). */
void ModeMenu_PlateGoto(s32 movie, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gModeMenu->flash[movie];

    if (kind == 0) {
        sprintf(name, "mc_menu_plate_%d", gModeMenu->cursor[0] - gModeMenu->top + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipGotoLabel(flash, &ref, label);
    }
}

#define MODEMENU_VOICE(line) \
    gModeMenu->voiceLine = (line); \
    Voice_PlayWithSubtitle(gModeMenu->subtitles, gModeMenu->voiceBase, gModeMenu->voiceLine)

/* The cursor moved: light its plate, let the guide present the item and load its picture. */
#define MODEMENU_MOVED() \
    ModeMenu_PlateGoto(0, 0, "fl_on_start"); \
    MODEMENU_VOICE(MODEMENU_ITEM(gModeMenu) + gModeMenu->lineBase + 4); \
    ModeMenu_ChangeImage(); \
    Snd_PlaySe(1, 0)

/* Pad handling: the list (focus 0) or the description of the chosen item (focus 1). */
void ModeMenu_Input(s32 *result) {
    if (!(gModeMenu->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gModeMenu->flags & MODEMENU_STARTED)) {
        ModeMenu_PlateGoto(0, 0, "fl_on_start");
        gModeMenu->flags |= MODEMENU_STARTED;
    }
    switch (gModeMenu->focus) {
    case 0:
        if (gPad[0].gameRepeat & 1) {
            gModeMenu->unk124 = 0;
            if (gModeMenu->cursor[gModeMenu->focus] > 0 &&
                gModeMenu->items[gModeMenu->cursor[gModeMenu->focus] - 1] != gModeMenu->itemMax) {
                ModeMenu_PlateGoto(0, 0, "fl_off_start");
                gModeMenu->cursor[gModeMenu->focus]--;
                if (gModeMenu->cursor[gModeMenu->focus] < gModeMenu->top) {
                    Flash_GotoLabel(&gModeMenu->flash[0], "fl_battle_right", 1);
                    gModeMenu->extra = gModeMenu->bottom;
                    gModeMenu->top--;
                    gModeMenu->bottom--;
                }
                MODEMENU_MOVED();
            }
        } else if (gPad[0].gameRepeat & 2) {
            gModeMenu->unk124 = 0;
            if (gModeMenu->cursor[gModeMenu->focus] < gModeMenu->itemCount - 1 &&
                gModeMenu->items[gModeMenu->cursor[gModeMenu->focus] + 1] != gModeMenu->itemMax) {
                ModeMenu_PlateGoto(0, 0, "fl_off_start");
                gModeMenu->cursor[gModeMenu->focus]++;
                if (gModeMenu->cursor[gModeMenu->focus] > gModeMenu->bottom) {
                    Flash_GotoLabel(&gModeMenu->flash[0], "fl_battle_left", 1);
                    gModeMenu->extra = gModeMenu->top;
                    gModeMenu->top++;
                    gModeMenu->bottom++;
                }
                MODEMENU_MOVED();
            }
        } else if (gPad[0].gamePressed & 0x200) {
            gModeMenu->unk124 = 0;
            ModeMenu_PlateGoto(0, 0, "fl_ok");
            gModeMenu->step = MODEMENU_STEP_GUIDE;
            gModeMenu->scroll = 0.0f;
            gModeMenu->scrollMax = gModeMenu->descr[MODEMENU_ITEM(gModeMenu)].count * 40.0f + 160.0f;
            gModeMenu->flags &= ~MODEMENU_NEXT;
            gModeMenu->focus = 1;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gModeMenu->unk124 = 0;
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Snd_PlaySe(1, 2);
        }
        break;
    case 1:
        if (gPad[0].gamePressed & 0x1200) {
            gModeMenu->flags |= MODEMENU_CHOSEN;
            gModeMenu->flags |= MODEMENU_LEAVING;
            gModeMenu->timer = 15;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            ModeMenu_PlateGoto(0, 0, "fl_on_start");
            Flash_GotoLabel(&gModeMenu->flash[0], "fl_battle_cansel", 1);
            MsgWin_Open();
            MODEMENU_VOICE(gModeMenu->cursor[0] + gModeMenu->lineBase + 4);
            gModeMenu->focus = 0;
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

/* The sequence that follows the choice of an item: guide comment, description scrolled in and read out. */
void ModeMenu_UpdateStep(void) {
    if (gModeMenu->step == 0) {
        return;
    }
    switch (gModeMenu->step) {
    case MODEMENU_STEP_GUIDE:
        MODEMENU_VOICE(gModeMenu->lineBase + 1);
        gModeMenu->step++;
        break;
    case MODEMENU_STEP_GUIDE_WAIT:
        if (Voice_GetStat(0) == 5) {
            gModeMenu->step++;
        } else if (gPad[0].gamePressed & 0x200) {
            gModeMenu->step++;
            Voice_StopWithLip();
            Snd_PlaySe(1, 1);
        }
        break;
    case MODEMENU_STEP_OPEN:
        Flash_GotoLabel(&gModeMenu->flash[0], "fl_battle_ok", 1);
        MsgWin_Close();
        gModeMenu->step++;
        break;
    case MODEMENU_STEP_OPENED:
        gModeMenu->step = MODEMENU_STEP_NARR_INIT;
        break;
    case MODEMENU_STEP_NARR_INIT:
        gModeMenu->narrWait = 0;
        gModeMenu->narrLine = 0;
        gModeMenu->step++;
        break;
    case MODEMENU_STEP_NARR_START:
        if (gModeMenu->flash[0].trig & 1) {
            gModeMenu->flags |= MODEMENU_SCROLLING;
            gModeMenu->step++;
        }
        break;
    case MODEMENU_STEP_NARR_LINE:
        if (gModeMenu->descr[MODEMENU_ITEM(gModeMenu)].delay[gModeMenu->narrLine] <= gModeMenu->narrWait++) {
            Voice_PlayWithSubtitle(NULL, MODEMENU_NARR_VOICE,
                                   gModeMenu->descr[MODEMENU_ITEM(gModeMenu)].line + gModeMenu->narrLine);
            gModeMenu->narrWait = 0;
            gModeMenu->step++;
        }
        break;
    case MODEMENU_STEP_NARR_WAIT:
        if (Voice_GetStat(0) == 5) {
            if (gModeMenu->descr[MODEMENU_ITEM(gModeMenu)].count <= ++gModeMenu->narrLine) {
                gModeMenu->step++;
            } else {
                gModeMenu->step--;
            }
        } else if (gPad[0].gamePressed & 0x1000) {
            gModeMenu->step = MODEMENU_STEP_GO;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            ModeMenu_PlateGoto(0, 0, "fl_on_start");
            Flash_GotoLabel(&gModeMenu->flash[0], "fl_battle_cansel", 1);
            MsgWin_Open();
            MODEMENU_VOICE(gModeMenu->cursor[0] + gModeMenu->lineBase + 4);
            gModeMenu->narrWait = 0;
            gModeMenu->narrLine = 0;
            gModeMenu->step++;
            gModeMenu->focus = 0;
            Snd_PlaySe(1, 2);
        }
        break;
    case MODEMENU_STEP_NARR_END:
        gModeMenu->step = 0;
        break;
    case MODEMENU_STEP_GO:
        gModeMenu->step = 0;
        gModeMenu->flags |= MODEMENU_CHOSEN;
        gModeMenu->flags |= MODEMENU_LEAVING;
        gModeMenu->timer = 15;
        break;
    }
}

/* The sub menu's own frame loop. Returns 1 when a battle was chosen (the battle setup is prepared), 0 on cancel. */
s32 ModeMenu_Run(s32 section) {
    s32 result = 1;

    ModeMenu_Init(section);
    if (gProgress->prevMode == 6) {
        ColorFade_StartIn(0xFF, 0xFF, 0xFF, 0x14);
    } else {
        ColorFade_StartIn(0, 0, 0, 0x14);
    }
    while (1) {
        Gfx_BeginFrame();
        ModeMenu_UpdateImage();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            ModeMenu_Update();
            ModeMenu_UpdateStep();
        }
        ModeMenu_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gModeMenu->flags & MODEMENU_GREETED) && (gModeMenu->flash[0].flags & MFLASH_PAD)) {
                gModeMenu->flags |= MODEMENU_GREETED;
                {
                    SaveSlot *slot = &gSaveData->slot[gProgress->subMenu];

                    if (slot->flags & SAVESLOT_GREET_FIRST) {
                        gModeMenu->voiceLine = gModeMenu->lineBase + 3;
                        slot = gSaveData->slot;
                        slot += gProgress->subMenu;
                        slot->flags &= ~SAVESLOT_GREET_FIRST;
                    } else {
                        gModeMenu->voiceLine = gModeMenu->lineBase;
                    }
                }
                Voice_PlayWithSubtitle(gModeMenu->subtitles, gModeMenu->voiceBase, gModeMenu->voiceLine);
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            StreamSe_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gModeMenu->loadState != MODEMENU_LOAD_SHOWN) {
                continue;
            }
            break;
        } else if (gModeMenu->flags & MODEMENU_LEAVING) {
            if (--gModeMenu->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                gProgress->subMenuItem = MODEMENU_ITEM(gModeMenu);
            }
        } else if (gModeMenu->step == 0) {
            ModeMenu_Input(&result);
        }
    }
    if (result) {
        gSaveData->slot[gProgress->subMenu].val[2] &= ~(1 << MODEMENU_ITEM(gModeMenu));
        BattleSetup_Clear();
        BattleSetup_SetScript(ModeMenu_GetLine(gProgress->subMenu, MODEMENU_ITEM(gModeMenu)));
    }
    ModeMenu_Term();
    Dma_ResetBuffers();
    return result;
}
