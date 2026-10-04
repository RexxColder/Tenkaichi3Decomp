#include "common.h"
#include "menu/menu_s.h"
#include "sys/pad.h"

/*
 * SimTop, 0x388618..0x388EE0: the last five functions of the entry screen of the sim sub family (mode 20). The
 * object starts in the previous chunk (menu_r_d.c: 0x387880 the star helper, 0x387900 Init, 0x387F20 Term,
 * 0x387F90 Draw); its work pointer is 0x3B7420. Append this file to that one. Read-only data emitted here: the
 * jump table of SimTop_UpdateVoice (0x3BA050) and the strings 0x3BA068..0x3BA138.
 *
 * The object's read-only data starts at 0x3B9ED0 with three file-scope constant tables, which the merged file
 * has to define above its first function (checked: menu_r_d.c + this file with these three definitions links
 * byte-identical, text 0x387880..0x388EE0 and .rodata 0x3B9ED0..0x3BA138):
 *     const s32 gSimTopUnused[1] = { 26 };          0x3B9ED0, not referenced (26 = the cursor row's texture slot)
 *     const s32 gSimTopPlateVoice[3] = { 1, 2, 3 }; 0x3B9ED8
 *     const s32 gSimTopIdleVoice[2] = { 4, 5 };     0x3B9EE8
 * "mc_sim_plate%02d" (SimTop_ClipGoto) is shared with SimTop_Draw's copy at 0x3B9F50; compiled alone this file
 * emits it once more behind its last string.
 */

/* Runs the leave timer and the movie. */
void SimTop_Update(void) {
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gSimTop->timer > 0) {
        gSimTop->timer--;
    }
    for (i = 0; i < SIMTOP_FLASH_NUM; i++) {
        Flash_Advance(&gSimTop->flash[i]);
    }
}

/* Starts the line the guide was asked for; confirm cuts the running line short. */
void SimTop_UpdateVoice(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gSimTop->voiceSkip == 0) {
        if (gSimTop->voiceReq == 0) {
            return;
        }
        if (gSimTop->voiceLine != -1 && Voice_GetStat(0) != S_VOICE_IDLE && gSimTop->voiceReq == gSimTop->voiceLast) {
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
                gSimTop->voiceSkip = 1;
            }
            return;
        }
    } else {
        gSimTop->voiceSkip = 0;
    }
    switch (gSimTop->voiceReq) {
    case 1:
        gSimTop->voiceLine = 0x11;
        break;
    case 2:
        gSimTop->voiceLine = 0x12;
        break;
    case 3:
        gSimTop->voiceLine = 0x13;
        break;
    case 4:
        gSimTop->voiceLine = 0x14;
        break;
    case 5:
        gSimTop->voiceLine = 0x15;
        break;
    }
    gSimTop->voiceReq = 0;
    Voice_PlayWithSubtitle(gSimTop->subtitles, S_VOICE_BASE, gSimTop->voiceLine);
    gSimTop->voiceLast = gSimTop->voiceReq;
}

/*
 * Pad 0. Level 0: up / down the plate, confirm opens it (plate 0 starts a run), cancel leaves; level 1 scrolls
 * the ranking; level 2 pages through the "how to play" text.
 */
void SimTop_Input(s32 *result) {
    s32 up = gPad[0].gameRepeat & PADG_UP;
    s32 down = gPad[0].gameRepeat & PADG_DOWN;
    s32 right = gPad[0].gameRepeat & PADG_RIGHT;
    s32 confirm = gPad[0].gamePressed & PADG_CROSS;
    s32 cancel = gPad[0].gamePressed & PADG_TRIANGLE;

    if (!(gSimTop->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gSimTop->flags & SIMTOP_STARTED)) {
        SimTop_ClipGoto(0, 0, "fl_on_start");
        gSimTop->voiceReq = 1;
        gSimTop->flags |= SIMTOP_STARTED;
    }
    switch (gSimTop->level) {
    case SIMTOP_LV_MENU:
        if (up) {
            SimTop_ClipGoto(0, 0, "fl_off_start");
            if (--gSimTop->cur[gSimTop->level] < 0) {
                gSimTop->cur[gSimTop->level] = SIMTOP_ROWS - 1;
            }
            SimTop_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimTop->voiceReq = gSimTopPlateVoice[gSimTop->cur[gSimTop->level]];
            gSimTop->idle = 0;
        } else if (down) {
            SimTop_ClipGoto(0, 0, "fl_off_start");
            if (++gSimTop->cur[gSimTop->level] >= SIMTOP_ROWS) {
                gSimTop->cur[gSimTop->level] = 0;
            }
            SimTop_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimTop->voiceReq = gSimTopPlateVoice[gSimTop->cur[gSimTop->level]];
            gSimTop->idle = 0;
        } else if (confirm) {
            switch (gSimTop->cur[0]) {
            case 0:
                gSimTop->flags |= SIMTOP_CHOSEN;
                gSimTop->flags |= SIMTOP_LEAVING;
                gSimTop->timer = 15;
                break;
            case 1:
                gSimTop->level = SIMTOP_LV_RANKING;
                Flash_GotoLabel(&gSimTop->flash[0], "fl_senreki_plate_in", 1);
                break;
            case 2:
                gSimTop->level = SIMTOP_LV_HELP;
                gSimTop->page = 0;
                Flash_GotoLabel(&gSimTop->flash[0], "fl_asobikata_plate_in", 1);
                break;
            }
            Voice_StopWithLip();
            Snd_PlaySe(1, 1);
            gSimTop->idle = 0;
        } else if (cancel) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Flash_GotoLabel(&gSimTop->flash[0], "fl_sim_plate_cansel", 1);
            Snd_PlaySe(1, 2);
        } else {
            gSimTop->idle++;
            if (gSimTop->idle == SIMTOP_IDLE_FRAMES) {
                gSimTop->idle = 0;
                gSimTop->voiceReq = gSimTopIdleVoice[Rand_Range(2)];
            }
        }
        break;
    case SIMTOP_LV_RANKING:
        if (up && gSimTop->top != 0) {
            Flash_GotoLabel(&gSimTop->flash[0], "fl_senreki_plate_down", 1);
            gSimTop->top--;
            gSimTop->cursor = gSimTop->top + 5;
            Snd_PlaySe(1, 0);
        } else if (down && (u32)gSimTop->top < SIMTOP_RANK_TOP_MAX) {
            Flash_GotoLabel(&gSimTop->flash[0], "fl_senreki_plate_up", 1);
            gSimTop->top++;
            gSimTop->cursor = gSimTop->top - 1;
            Snd_PlaySe(1, 0);
        } else if (cancel) {
            gSimTop->level = SIMTOP_LV_MENU;
            Flash_GotoLabel(&gSimTop->flash[0], "fl_senreki_plate_cansel", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    case SIMTOP_LV_HELP:
        if (confirm || right) {
            if (gSimTop->page < SIMTOP_HELP_PAGES) {
                gSimTop->page++;
            } else {
                gSimTop->level = SIMTOP_LV_MENU;
                Flash_GotoLabel(&gSimTop->flash[0], "fl_asobikata_plate_cansel", 1);
            }
            Snd_PlaySe(1, 1);
        } else if (cancel) {
            gSimTop->level = SIMTOP_LV_MENU;
            Flash_GotoLabel(&gSimTop->flash[0], "fl_asobikata_plate_cansel", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

/* Sends the plate under the cursor of `level` to a label. */
void SimTop_ClipGoto(s32 movie, s32 level, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gSimTop->flash[movie];

    sprintf(name, "mc_sim_plate%02d", gSimTop->cur[level] + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/*
 * The entry screen of the sim sub family (mode 20). Returns 1 when "start" was chosen (Ub_Main goes on to the
 * character select, mode 21), 0 when the player backed out (mode 13).
 */
s32 SimTop_Run(s32 section) {
    s32 result = 1;

    SimTop_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        SimTop_Update();
        SimTop_UpdateVoice();
        SimTop_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gSimTop->flags & SIMTOP_GREETED) && (gSimTop->flash[0].flags & MFLASH_PAD)) {
                gSimTop->flags |= SIMTOP_GREETED;
            }
        }
        if (ColorFade_IsFadingOut()) {
            Bgm_FadeOutStep();
            Voice_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gSimTop->flags & SIMTOP_LEAVING) {
            if (gSimTop->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gSimTop->voiceReq == 0) {
            SimTop_Input(&result);
        }
    }
    SimTop_Term();
    Dma_ResetBuffers();
    return result;
}
