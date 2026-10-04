#include "common.h"
#include "menu/menu_n.h"
#include "sys/pad.h"

/*
 * UbTeamSel, 0x372148..0x372560: the last two functions of the team select of the mode group 13..30 (the object
 * starts at 0x36E028, src/menu/menu_m_b.c; work pointer 0x3B734C; its .rodata ends at 0x3B7ADC with the jump
 * table of its pad handler). Nothing here emits read-only data. Append this file to menu_m_b.c; NTeamSel is a
 * local view of that file's UbTeamSel (include/menu/menu_m.h was still changing when this was written).
 */

/* The head of this object (previous chunk, menu_m_b.c). */
extern void UbTeamSel_UpdateFaceLoad(void);
extern void UbTeamSel_Init(s32 section);
extern void UbTeamSel_Term(void);
extern void UbTeamSel_Draw(void);
extern void UbTeamSel_Update(void);
extern void UbTeamSel_Input(s32 *result);

extern NTeamSel *gUbTeamSel; /* 0x3B734C */

/* Once the team is chosen: the guide's closing line, then the leave timer. */
void UbTeamSel_UpdateEnd(void) {
    s32 step = gUbTeamSel->endStep;

    if (step == NTEAM_END_NONE) {
        return;
    }
    switch (step) {
    case NTEAM_END_SPEAK:
        gUbTeamSel->talker = 0;
        gUbTeamSel->voiceLine = 0x37;
        Voice_PlayWithSubtitle(gUbTeamSel->subtitles, N_VOICE_BASE, gUbTeamSel->voiceLine);
        gUbTeamSel->endStep++;
        break;
    case NTEAM_END_WAIT:
        if (Voice_GetStat(0) == N_VOICE_IDLE) {
            gUbTeamSel->endStep++;
        } else if (gPad[0].gamePressed & 0x200) {
            gUbTeamSel->endStep++;
            Snd_PlaySe(1, 1);
        }
        break;
    case NTEAM_END_LEAVE:
        gUbTeamSel->flags |= NTEAM_DONE;
        gUbTeamSel->flags |= NTEAM_LEAVING;
        gUbTeamSel->timer = 15;
        gUbTeamSel->endStep = NTEAM_END_NONE;
        break;
    }
}

/*
 * The team select (mode 15 when gProgress->teamSize >= 2). Returns 1 when a team was chosen (the members are then
 * in gProgress->team and their number in gProgress->teamSize), 0 when the player backed out.
 */
s32 UbTeamSel_Run(s32 section) {
    s32 result = 1;

    UbTeamSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        UbTeamSel_UpdateFaceLoad();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            UbTeamSel_Update();
            UbTeamSel_UpdateEnd();
        }
        UbTeamSel_Draw();
        Font_FlushAll();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gUbTeamSel->flags & NTEAM_GREETED) && (gUbTeamSel->flash[0].flags & MFLASH_PAD)) {
                gUbTeamSel->flags |= NTEAM_GREETED;
                gUbTeamSel->talker = 0;
                gUbTeamSel->voiceLine = 0x36;
                Voice_PlayWithSubtitle(gUbTeamSel->subtitles, N_VOICE_BASE, gUbTeamSel->voiceLine);
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gUbTeamSel->loadState != NTEAM_LOAD_IDLE) {
                continue;
            }
            break;
        }
        if (gUbTeamSel->flags & NTEAM_LEAVING) {
            if (--gUbTeamSel->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                /* hand the choice to the mode */
                NPROG_TEAM = gUbTeamSel->sel->team;
            }
        } else if (gUbTeamSel->endStep == NTEAM_END_NONE) {
            UbTeamSel_Input(&result);
        }
    }
    if (result) {
        NPROG->teamSize = gUbTeamSel->sel->memberCount;
    }
    UbTeamSel_Term();
    Dma_ResetBuffers();
    return result;
}
