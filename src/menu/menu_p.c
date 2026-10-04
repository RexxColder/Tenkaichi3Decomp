#include "common.h"
#include "menu/menu_p.h"
#include "sys/pad.h"

/*
 * MisSel, 0x37AFF8..0x37B7C0: the last five functions of the mission select (mode 14). The object starts in the
 * previous chunk (src/menu/menu_o_d.c, 0x379F58: SetRank, SetupBattle, Init, Term, Draw); its work pointer is
 * gMisSel (0x3B7360) and its .rodata runs 0x3B8400..0x3B8630. Append this file to that one: ClipGoto's
 * "mc_window_plate%02d" is the head's string at 0x3B8400.
 */

#define MS gMisSel

/* The head of this object (previous chunk). */
extern void MisSel_SetupBattle(void);    /* rule and opponents of the chosen mission */
extern void MisSel_Init(s32 section);
extern void MisSel_Term(void);
extern void MisSel_Draw(void);

void MisSel_ClipGoto(s32 movie, s32 level, char *label);

/* Runs the leave timer and the movie. */
void MisSel_Update(void) {
    s32 i;

    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (MS->timer > 0) {
        MS->timer--;
    }
    for (i = 0; i < MISSEL_FLASH_NUM; i++) {
        Flash_Advance(&MS->flash[i]);
    }
}

/* Starts the line the guide was asked for; confirm cuts the running line short. */
void MisSel_UpdateVoice(void) {
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (MS->voiceSkip == 0) {
        if (MS->voiceReq == 0) {
            return;
        }
        if (MS->voiceLine != -1 && Voice_GetStat(0) != P_VOICE_IDLE && MS->voiceReq == MS->voiceLast) {
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
                MS->voiceSkip = 1;
            }
            return;
        }
    } else {
        MS->voiceSkip = 0;
    }
    switch (MS->voiceReq) {
    case 1:
        MS->voiceLine = 0x33;
        break;
    case 2:
        MS->voiceLine = 0x35;
        break;
    }
    MS->voiceReq = 0;
    Voice_PlayWithSubtitle(MS->subtitles, P_VOICE_BASE, MS->voiceLine);
    MS->voiceLast = MS->voiceReq;
}

/* Pad 0. Level 0: up / down the plate, left / right the page; level 1: confirm or back. */
void MisSel_Input(s32 *result) {
    char name[64];
    s32 up = gPad[0].gameRepeat & PADG_UP;
    s32 down = gPad[0].gameRepeat & PADG_DOWN;
    s32 left = gPad[0].gameRepeat & PADG_LEFT;
    s32 right = gPad[0].gameRepeat & PADG_RIGHT;
    s32 confirm = gPad[0].gamePressed & PADG_CROSS;
    s32 cancel = gPad[0].gamePressed & PADG_TRIANGLE;

    if (!(MS->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(MS->flags & MISSEL_STARTED)) {
        MS->voiceReq = 1;
        MisSel_ClipGoto(0, 0, "fl_on_start");
        MS->flags |= MISSEL_STARTED;
    }
    switch (MS->level) {
    case 0:
        if (up) {
            MisSel_ClipGoto(0, 0, "fl_off_start");
            if (--MS->cur[MS->level] < 0) {
                MS->cur[MS->level] = MISSEL_ROWS - 1;
            }
            MisSel_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            MS->idle = 0;
        } else if (down) {
            MisSel_ClipGoto(0, 0, "fl_off_start");
            if (++MS->cur[MS->level] >= MISSEL_ROWS) {
                MS->cur[MS->level] = 0;
            }
            MisSel_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            MS->idle = 0;
        } else if (left) {
            if (MS->rank != 0) {
                MS->rank--;
                Snd_PlaySe(1, 0);
                MS->idle = 0;
            }
        } else if (right) {
            if ((u32)MS->rank < (u32)(MS->rankCount - 1)) {
                MS->rank++;
                Snd_PlaySe(1, 0);
                MS->idle = 0;
            }
        } else if (confirm) {
            MS->mission = MS->rank * MISSEL_ROWS + MS->cur[0];
            MisSel_ClipGoto(0, 0, "fl_rank_out");
            sprintf(name, "fl_mission_%02d_in", MS->cur[MS->level] + 1);
            Flash_GotoLabel(&MS->flash[0], name, 1);
            MS->level = 1;
            Snd_PlaySe(1, 1);
            MS->idle = 0;
        } else if (cancel) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Flash_GotoLabel(&MS->flash[0], "fl_mission100_menu_cansel", 1);
            Snd_PlaySe(1, 2);
            MS->idle = 0;
        } else {
            MS->idle++;
            if (MS->idle == MISSEL_IDLE_FRAMES) {
                MS->voiceReq = 2;
                MS->idle = 0;
            }
        }
        break;
    case 1:
        if (confirm) {
            MS->flags |= MISSEL_CHOSEN;
            MS->flags |= MISSEL_LEAVING;
            MS->timer = 15;
            P_PROG->misRank = MS->rank;
            P_PROG->misRow = MS->cur[0];
            Snd_PlaySe(1, 1);
        } else if (cancel) {
            MS->level = 0;
            MisSel_ClipGoto(0, 0, "fl_rank_in");
            sprintf(name, "fl_mission_%02d_cansel", MS->cur[MS->level] + 1);
            Flash_GotoLabel(&MS->flash[0], name, 1);
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

/* Sends the plate under the cursor of `level` to a label. */
void MisSel_ClipGoto(s32 movie, s32 level, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &MS->flash[movie];

    sprintf(name, "mc_window_plate%02d", MS->cur[level] + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/*
 * The mission select (mode 14). Returns 1 when a mission was chosen (its page and plate are then in gProgress and
 * the rule and the opponents were written to the battle setup by MisSel_SetupBattle), 0 when the player backed out.
 */
s32 MisSel_Run(s32 section) {
    s32 result = 1;

    MisSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        MisSel_Update();
        MisSel_UpdateVoice();
        MisSel_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(MS->flags & MISSEL_GREETED) && (MS->flash[0].flags & MFLASH_PAD)) {
                MS->flags |= MISSEL_GREETED;
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
        if (MS->flags & MISSEL_LEAVING) {
            if (MS->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (MS->voiceReq == 0) {
            MisSel_Input(&result);
        }
    }
    if (result) {
        MisSel_SetupBattle();
    }
    MisSel_Term();
    Dma_ResetBuffers();
    return result;
}
