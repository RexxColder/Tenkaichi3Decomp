#include "common.h"
#include "menu/menu_k.h"
#include "sys/pad.h"
#include "sys/save.h"

/*
 * TourMenu, 0x364358..0x364DA8: tail of the tournament menu (progress mode 33). The object starts at 0x3623A8
 * (menu_j_b.c): this file has to be appended to it (shared strings "fl_menu_in", "fl_on_start", ...).
 */

#define TM_SAY(n) Voice_PlayWithSubtitle(gTourMenu->subtitles, TOUR_VOICE_BASE, n)

/* Steps the guide's scripted speech: `seq` is the step, 0 = idle (the menu takes input). */
void TourMenu_UpdateSeq(void) {
    if (gTourMenu->seq == 0) {
        return;
    }
    switch (gTourMenu->seq) {
    /* 1..7: first visit, three lines */
    case 1:
        gTourMenu->voiceLine = 0;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 3:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 5:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 7:
        gSaveData->unkA08 |= TOUR_SAVE_STARTED;
        if (gTourMenu->flags & TOURMENU_INVITE) {
            gTourMenu->voiceLine = -1;
            Voice_StopWithLip();
            gTourMenu->seq = 0x33;
        } else {
            gTourMenu->voiceLine = 3;
            TM_SAY(gTourMenu->voiceLine);
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_menu_in", 1);
            TourMenu_ClipGoto(0, 0, "fl_on_start");
            gTourMenu->seq = 0;
        }
        break;
    case 2:
    case 4:
    case 6:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    /* 0x33..0x39: an invitation arrived */
    case 0x33:
        Flash_Play(&gTourMenu->flash[1], 1);
        Flash_GotoLabel(&gTourMenu->flash[1], "fl_taikai_syotai_in", 1);
        gTourMenu->seq++;
        break;
    case 0x34:
        if (gTourMenu->flash[1].trig & 1) {
            gTourMenu->seq = 0x35;
        }
        break;
    case 0x35:
        gTourMenu->voiceLine = 4;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x36:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x37:
        Flash_GotoLabel(&gTourMenu->flash[1], "fl_taikai_syotai_out", 1);
        gTourMenu->seq++;
        break;
    case 0x38:
        if (gTourMenu->flash[1].trig & 1) {
            gTourMenu->seq++;
        }
        break;
    case 0x39:
        if (!(gSaveData->unkA08 & TOUR_SAVE_EXPLAINED)) {
            gTourMenu->seq = 0x65;
        } else {
            gTourMenu->voiceLine = 3;
            TM_SAY(gTourMenu->voiceLine);
            Flash_GotoLabel(&gTourMenu->flash[0], "fl_menu_in", 1);
            TourMenu_ClipGoto(0, 0, "fl_on_start");
            gTourMenu->seq = 0;
        }
        break;
    /* 0x65..0x69: the long explanation, once */
    case 0x65:
        gTourMenu->voiceLine = 5;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x67:
        gTourMenu->voiceLine = 9;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x66:
    case 0x68:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x69:
        gSaveData->unkA08 |= TOUR_SAVE_EXPLAINED;
        gTourMenu->voiceLine = 3;
        TM_SAY(gTourMenu->voiceLine);
        Flash_GotoLabel(&gTourMenu->flash[0], "fl_menu_in", 1);
        TourMenu_ClipGoto(0, 0, "fl_on_start");
        gTourMenu->seq = 0;
        break;
    /* 0x97.., 0xC9.., 0xFB.., 0x12D.., 0x15F..: the description of one tournament, two lines; cancel leaves */
    case 0x97:
        gTourMenu->voiceLine = 0x12;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x98:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gTourMenu->seq = 0x191;
            Snd_PlaySe(1, 2);
        }
        break;
    case 0x99:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq = 0;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0xC9:
        gTourMenu->voiceLine = 0x14;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0xCA:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gTourMenu->seq = 0x191;
            Snd_PlaySe(1, 2);
        }
        break;
    case 0xCB:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq = 0;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0xFB:
        gTourMenu->voiceLine = 0x16;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0xFC:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gTourMenu->seq = 0x191;
            Snd_PlaySe(1, 2);
        }
        break;
    case 0xFD:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq = 0;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x12D:
        gTourMenu->voiceLine = 0x18;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x12E:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gTourMenu->seq = 0x191;
            Snd_PlaySe(1, 2);
        }
        break;
    case 0x12F:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq = 0;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x15F:
        gTourMenu->voiceLine = 0x1A;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x160:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gTourMenu->seq = 0x191;
            Snd_PlaySe(1, 2);
        }
        break;
    case 0x161:
        gTourMenu->voiceLine = gTourMenu->voiceLine + 1;
        gTourMenu->seq = 0;
        TM_SAY(gTourMenu->voiceLine);
        break;
    /* the description was cancelled: back to the tournament list */
    case 0x191:
        gTourMenu->seq = 0;
        gTourMenu->voiceLine = -1;
        Voice_StopWithLip();
        Flash_GotoLabel(&gTourMenu->flash[0], "fl_taikai_setumei_cancel", 1);
        TourMenu_ClipGoto(0, 2, "fl_off_start");
        gTourMenu->level = TOURMENU_LV_TOUR;
        break;
    /* 0x1C3..0x1C5: everything is chosen: the guide's send-off, then leave */
    case 0x1C3:
        gTourMenu->voiceLine = 0x21;
        gTourMenu->seq++;
        TM_SAY(gTourMenu->voiceLine);
        break;
    case 0x1C4:
        if (Voice_GetStat(0) == MVOICE_IDLE) {
            gTourMenu->seq++;
        } else if (gPad[0].gamePressed & 0x200) {
            gTourMenu->seq++;
            Snd_PlaySe(1, 1);
        }
        break;
    case 0x1C5:
        gTourMenu->seq = 0;
        gTourMenu->flags |= TOURMENU_DONE;
        gTourMenu->flags |= TOURMENU_LEAVING;
        gTourMenu->leaveTimer = 0xF;
        break;
    }
}

/* Runs the tournament menu until it fades out; stores the choices in gProgress. Returns what TourMenu_Input left in
 * `result` (1 unless the menu was cancelled). */
s32 TourMenu_Run(s32 section) {
    s32 result = 1;

    TourMenu_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            TourMenu_Update();
            TourMenu_UpdateSeq();
        }
        TourMenu_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (!(gTourMenu->flags & TOURMENU_GREETED) && (gTourMenu->flash[0].flags & MFLASH_PAD)) {
            gTourMenu->flags |= TOURMENU_GREETED;
            if (!(gSaveData->unkA08 & TOUR_SAVE_STARTED)) {
                gTourMenu->seq = 1;
            } else if (gTourMenu->flags & TOURMENU_INVITE) {
                gTourMenu->seq = 0x33;
            } else {
                gTourMenu->voiceLine = 3;
                TM_SAY(gTourMenu->voiceLine);
                Flash_GotoLabel(&gTourMenu->flash[0], "fl_menu_in", 1);
                TourMenu_ClipGoto(0, 0, "fl_on_start");
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            StreamSe_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gTourMenu->flags & TOURMENU_LEAVING) {
            if (--gTourMenu->leaveTimer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                TOUR_PROG2->entry = gTourMenu->cursor[TOURMENU_LV_TOP];
                TOUR_PROG2->tour = gTourMenu->cursor[TOURMENU_LV_TOUR];
                TOUR_PROG2->level = gTourMenu->cursor[TOURMENU_LV_LEVEL];
                TOUR_PROG2->entryNum = gTourMenu->cursor[TOURMENU_LV_NUM] + 1;
            }
        } else if (gTourMenu->seq == 0) {
            TourMenu_Input(&result);
        }
    }
    TourMenu_Term();
    Dma_ResetBuffers();
    return result;
}
