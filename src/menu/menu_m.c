#include "common.h"
#include "menu/menu_m.h"
#include "sys/pad.h"

/*
 * SoloSel, 0x36DBE8..0x36E028: the last two functions of the one-character select of the mode group 13..30
 * (the object starts in the previous chunk, menu_l; its work pointer is 0x3B7348 and its .rodata ends at
 * 0x3B7738 with the jump table of its pad handler). Nothing here emits read-only data.
 */

/* The head of this object (previous chunk; names from config/symbols/menu_l.txt). */
extern void SoloSel_UpdateImage(void);  /* portrait loader step */
extern void SoloSel_Init(s32 section);
extern void SoloSel_Term(void);
extern void SoloSel_Draw(void);
extern void SoloSel_Update(void);
extern void SoloSel_Input(s32 *result);

/* Once the fighter is chosen: the guide's closing line, then the leave timer. */
void SoloSel_UpdateEnd(void) {
    s32 step = gSoloSel->endStep;

    if (step == UBSEL_END_NONE) {
        return;
    }
    switch (step) {
    case UBSEL_END_SPEAK:
        if (gProgress->mode == 21) {
            gSoloSel->talker = 1;
            gSoloSel->voiceLine = 0x17;
            if (gSoloSel->sel->chara == 0x66) {
                gSoloSel->voiceLine++;
            }
        } else {
            gSoloSel->talker = 0;
            gSoloSel->voiceLine = 0x37;
        }
        Voice_PlayWithSubtitle(gSoloSel->subtitles, UB_VOICE_BASE, gSoloSel->voiceLine);
        gSoloSel->endStep++;
        break;
    case UBSEL_END_WAIT:
        if (Voice_GetStat(0) == UB_VOICE_IDLE) {
            gSoloSel->endStep++;
        } else if (gPad[0].gamePressed & 0x200) {
            gSoloSel->endStep++;
            Snd_PlaySe(1, 1);
        }
        break;
    case UBSEL_END_LEAVE:
        gSoloSel->flags |= UBSEL_DONE;
        gSoloSel->flags |= UBSEL_LEAVING;
        gSoloSel->timer = 15;
        gSoloSel->endStep = UBSEL_END_NONE;
        break;
    }
}

/*
 * The one-character select (modes 15 with a team size below 2, 18, 21, 25 and 29). Returns 1 when a fighter was
 * chosen (the choice is then in gProgress->team[0]), 0 when the player backed out.
 */
s32 SoloSel_Run(s32 section) {
    s32 result = 1;

    SoloSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        SoloSel_UpdateImage();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            SoloSel_Update();
            SoloSel_UpdateEnd();
        }
        SoloSel_Draw();
        Font_FlushAll();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gSoloSel->flags & UBSEL_GREETED) && (gSoloSel->flash[0].flags & MFLASH_PAD)) {
                gSoloSel->flags |= UBSEL_GREETED;
                if (gProgress->mode == 21) {
                    gSoloSel->talker = 1;
                    gSoloSel->voiceLine = 0x16;
                    Voice_PlayWithSubtitle(gSoloSel->subtitles, UB_VOICE_BASE, gSoloSel->voiceLine);
                } else {
                    gSoloSel->talker = 0;
                    gSoloSel->voiceLine = 0x36;
                    Voice_PlayWithSubtitle(gSoloSel->subtitles, UB_VOICE_BASE, gSoloSel->voiceLine);
                }
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gSoloSel->loadState != UBSEL_LOAD_IDLE) {
                continue;
            }
            break;
        }
        if (gSoloSel->flags & UBSEL_LEAVING) {
            if (--gSoloSel->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                /* hand the choice to the mode */
                UB_PROG->team.member[0] = gSoloSel->sel->member;
            }
        } else if (gSoloSel->endStep == UBSEL_END_NONE) {
            SoloSel_Input(&result);
        }
    }
    SoloSel_Term();
    Dma_ResetBuffers();
    return result;
}
