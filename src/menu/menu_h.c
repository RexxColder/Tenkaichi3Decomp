#include "common.h"
#include "menu/menu_h.h"

/*
 * 0x356090..0x3562B8: the frame loop of the duel menu (modes 38..41), the last function of the object that
 * the previous chunk (menu_g) covers. Its work area is the pointer at 0x3B38E8, which an earlier object
 * defines; the callees are that object's Init / Update / Input / Draw / Term.
 */

/* The previous chunk's functions (names from config/symbols/menu_g.txt; the last two are called where a second
   update and a second draw would be). */
extern void DuelMenu_Init(s32 section);
extern void DuelMenu_Term(void);
extern void DuelMenu_Draw(void);
extern void DuelMenu_Update(void);
extern void DuelMenu_Input(s32 *result);
extern void DuelMenu_UpdateStart(void);
extern void DuelMenu_UpdateReset(void);

#define DUELMENU_VOICE_BASE 0x85C7

/* Runs the screen until its fade out is over; returns what the input handler set (1 if it set nothing). */
s32 DuelMenu_Run(s32 section) {
    s32 result = 1;

    DuelMenu_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            DuelMenu_Update();
            DuelMenu_UpdateStart();
        }
        DuelMenu_Draw();
        DuelMenu_UpdateReset();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gDuelMenu->flags & DUELMENU_GREETED) && (gDuelMenu->flash[0].flags & MFLASH_PAD)) {
                gDuelMenu->flags |= DUELMENU_GREETED;
                gDuelMenu->unk14C = 0;
                gDuelMenu->voiceLine = 0;
                Voice_PlayWithSubtitle(gDuelMenu->subtitles, DUELMENU_VOICE_BASE, gDuelMenu->voiceLine);
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gDuelMenu->flags & DUELMENU_LEAVING) {
            if (--gDuelMenu->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                ((MenuProgressDuel *)gProgress)->unk620 = gDuelMenu->pick[0];
                ((MenuProgressDuel *)gProgress)->unk624 = gDuelMenu->pick[1];
                ((MenuProgressDuel *)gProgress)->unk630 = gDuelMenu->pick[2];
            }
        } else if (gDuelMenu->unk148 == 0 && gDuelMenu->unk184 == 0) {
            DuelMenu_Input(&result);
        }
    }
    DuelMenu_Term();
    Dma_ResetBuffers();
    return result;
}
