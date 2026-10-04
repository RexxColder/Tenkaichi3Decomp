#include "common.h"
#include "menu/menu_x.h"
#include "sys/pad.h"

/*
 * Menu overlay DBZP.BIN, 0x39EFC0..0x39FAA8: tail of the EvoTop object, the top menu of Evolution Z (progress
 * mode 48; guide Krillin, three plates). The object starts at 0x39EB08 in the previous chunk
 * (src/menu/menu_w_c.c: voice helper, plate helpers, movie advance, init); this file appends to it. The work
 * structure is a local of EvoTop_Run and is handed to every function.
 *
 * Read-only data: the object's pool runs 0x3BC008..0x3BC364. The strings first used here start at 0x3BC2C0
 * ("mc_bg_cloud_1" .. "fl_cancel"); "mc_menu_plate_%d" (0x3BC028) is shared with the head, and "fl_ok"
 * (0x3BC068) sits between the head's strings: it belongs to the inline helper EvoTop_PlateOk, which the original
 * therefore defined above EvoTop_Advance, although only EvoTop_Input (below) uses it. Appended to menu_w_c.c
 * with the helper moved there, the sixteen functions match and the pool is identical except for the eleven
 * unreferenced "host:" paths (checked in build/scratch_menu_x/src/menu/evotop_m.c).
 */

/*
 * Stand-ins for the head of the object: EvoTop_Input only matches when the compiler has seen the definition of
 * EvoTop_Wrap (it is side-effect free, which changes how the branch around its call is filled). They are
 * compiled, but the assembler skips their output, so this object still calls the real ones. Delete this block
 * when the file is appended to menu_w_c.c.
 */
ASM_STUB_BEGIN();
void EvoTop_PlayVoice(EvoTop *m, s32 line) {
    Voice_PlayWithSubtitle(m->subtitles, EVOTOP_VOICE_BASE, line);
    m->voiceLine = line;
}
s32 EvoTop_Wrap(s32 value, s32 min, s32 max) {
    if (value < min) {
        return max;
    }
    if (value > max) {
        return min;
    }
    return value;
}
void EvoTop_SetPlate(EvoTop *m, s32 on) {
    m->flags |= on;
}
ASM_STUB_END();

/* Frees the screen. */
void EvoTop_Term(EvoTop *m) {
    IconWin_Term();
    MsgWin_Term();
    Flash_Destroy(&m->flash[0]);
    if (m->res != NULL) {
        Heap_Free(m->res);
        m->res = NULL;
    }
    DcSave_Term();
}

/* Clip callback: scissor to the left cloud layer. */
void EvoTop_ScissorCloud1(void) {
    Sprite_SetScissor(0, 0x100, 0x12, 0x112);
}

/* Clip callback: scissor to the right cloud layer. */
void EvoTop_ScissorCloud2(void) {
    Sprite_SetScissor(0x180, 0x200, 0x12, 0x112);
}

/* Clip callback: scissor back to the whole screen. */
void EvoTop_ScissorOff(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* Hangs the scissor callbacks on the two cloud clips. */
void EvoTop_SetCloudClips(EvoTop *m) {
    MFlashRef ref;

    Flash_FindLabel(&m->flash[0], NULL, "mc_bg_cloud_1", &ref);
    Flash_ClipSetCallbackA(&m->flash[0], &ref, EvoTop_ScissorCloud1, NULL);
    Flash_ClipSetCallbackB(&m->flash[0], &ref, EvoTop_ScissorOff, NULL);
    Flash_FindLabel(&m->flash[0], NULL, "mc_bg_cloud_2", &ref);
    Flash_ClipSetCallbackA(&m->flash[0], &ref, EvoTop_ScissorCloud2, NULL);
    Flash_ClipSetCallbackB(&m->flash[0], &ref, EvoTop_ScissorOff, NULL);
}

/* Animates the guide's eyes and mouth. */
void EvoTop_UpdateFace(EvoTop *m) {
    MFlashRef ref;

    Flash_FindLabel(&m->flash[0], NULL, "mc_guide_cririn_eye", &ref);
    FlashAnim_Blink(&m->flash[0], &ref, &m->blink, 0);
    Flash_FindLabel(&m->flash[0], NULL, "mc_guide_cririn_mouth", &ref);
    if (Voice_GetStat(0) != MVOICE_IDLE && !DcSave_IsStarted()) {
        FlashAnim_Talk(&m->flash[0], &ref, &m->talk, 0);
    } else {
        FlashAnim_ShowNext2(&m->flash[0], &ref, 0);
    }
}

/* Starts the movie and the first voice line when due, then draws the screen. */
void EvoTop_Draw(EvoTop *m) {
    MFlashRef ref;
    MFlashUv uv;
    char name[0x100];
    s32 i;
    MFlash *flash;

    if (!(m->flags & EVOTOP_STARTED)) {
        Flash_GotoLabel(&m->flash[0], "fl_in", 1);
        m->flags |= EVOTOP_STARTED;
    }
    if (!(m->flags & EVOTOP_GREETED)) {
        if (m->flash[0].flags & MFLASH_PAD) {
            switch (m->cursor) {
            case -1:
                m->cursor = 0;
                EvoTop_PlayVoice(m, 0);
                break;
            case 0:
                EvoTop_PlayVoice(m, 1);
                break;
            case 1:
                EvoTop_PlayVoice(m, 2);
                break;
            case 2:
                EvoTop_PlayVoice(m, 3);
                break;
            }
            EvoTop_SetPlate(m, 1);
            m->flags |= EVOTOP_GREETED;
        }
    }
    EvoTop_UpdateFace(m);
    EvoTop_SetPlateText(m);
    EvoTop_SetCloudClips(m);
    flash = &m->flash[0];
    uv.x1 = 0x200;
    uv.y1 = 0x100;
    uv.x0 = 0;
    uv.y0 = 0;
    for (i = 0; i < 2; i++) {
        sprintf(name, "mc_bg_cloud_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        FlashAnim_Scroll(flash, &ref, &uv, &m->cloud, NULL, 0.28444445f, 0.0f);
    }
    Sprite_DrawPicture(m->bg, 0, 0, 0x80);
    for (i = 0; i < EVOTOP_FLASH_NUM; i++) {
        Flash_Draw(&m->flash[i]);
    }
    IconWin_Draw();
    MsgWin_Draw(0, 0, m->voiceLine);
    DcSave_Update();
}

/* Plays "fl_ok" on the cursor plate. Inline in the original: its locals are the first of EvoTop_Input's frame. */
static inline void EvoTop_PlateOk(EvoTop *m) {
    MFlashRef ref;
    char name[0x100];

    sprintf(name, "mc_menu_plate_%d", m->cursor + 1);
    Flash_FindLabel(&m->flash[0], NULL, name, &ref);
    Flash_ClipGotoLabel(&m->flash[0], &ref, "fl_ok");
}

/*
 * Pad 0 (gameRepeat up / down, gamePressed confirm / cancel), once the movie accepts input and no save flow
 * runs. Plates 0 and 1 leave with result 1 / 2; plate 2 starts the guide's explanation (voice lines 4..16,
 * advanced by confirm or when a line ends, cancel leaves it); cancel leaves with result 0 and, when
 * gProgress->flags bit 0 is set, starts the save flow first. After 3600 idle frames the guide says line 17.
 */
void EvoTop_Input(EvoTop *m) {
    if (!(m->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (m->flags & EVOTOP_CHOSEN) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (DcSave_GetState()) {
        return;
    }
    if ((gPad[0].gamePressed || Voice_GetStat(0) == MVOICE_IDLE) && m->voiceLine == 0) {
        EvoTop_PlayVoice(m, m->cursor + 1);
    }
    if (gPad[0].gameRepeat || gPad[0].gamePressed) {
        m->timer = 0;
    }
    if (!(m->flags & EVOTOP_EXPLAIN)) {
        if (gPad[0].gameRepeat & MPAD_UP) {
            EvoTop_SetPlate(m, 0);
            m->cursor--;
            m->cursor = EvoTop_Wrap(m->cursor, 0, 2);
            EvoTop_SetPlate(m, 1);
            Snd_PlaySe(1, 0);
            EvoTop_PlayVoice(m, m->cursor + 1);
        } else if (gPad[0].gameRepeat & MPAD_DOWN) {
            EvoTop_SetPlate(m, 0);
            m->cursor++;
            m->cursor = EvoTop_Wrap(m->cursor, 0, 2);
            EvoTop_SetPlate(m, 1);
            Snd_PlaySe(1, 0);
            EvoTop_PlayVoice(m, m->cursor + 1);
        } else if (gPad[0].gamePressed & MPAD_OK) {
            EvoTop_PlateOk(m);
            switch (m->cursor) {
            case 0:
                Flash_GotoLabel(&m->flash[0], "fl_out", 1);
                m->flags |= EVOTOP_CHOSEN;
                m->result = 1;
                break;
            case 1:
                Flash_GotoLabel(&m->flash[0], "fl_out", 1);
                m->flags |= EVOTOP_CHOSEN;
                m->result = 2;
                break;
            case 2:
                Flash_GotoLabel(&m->flash[0], "fl_evo_z_ets_in", 1);
                m->flags |= EVOTOP_EXPLAIN;
                EvoTop_PlayVoice(m, EVOTOP_LINE_EXPLAIN_FIRST);
                break;
            }
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & MPAD_CANCEL) {
            m->result = 0;
            m->flags |= EVOTOP_CHOSEN;
            Snd_PlaySe(1, 2);
            if (gProgress->flags & 1) {
                DcSave_Start();
            }
        }
        m->timer++;
    } else {
        if ((gPad[0].gamePressed & MPAD_OK) || Voice_GetStat(0) == MVOICE_IDLE) {
            if (m->voiceLine == 3) {
                EvoTop_PlateOk(m);
            }
            if (m->voiceLine == EVOTOP_LINE_EXPLAIN_LAST) {
                m->flags ^= EVOTOP_EXPLAIN;
                Flash_GotoLabel(&m->flash[0], "fl_evo_z_ets_out", 1);
                EvoTop_PlayVoice(m, 3);
            } else {
                m->voiceLine++;
                EvoTop_PlayVoice(m, m->voiceLine);
            }
            if (gPad[0].gamePressed & MPAD_OK) {
                Snd_PlaySe(1, 1);
            }
        } else if (gPad[0].gamePressed & MPAD_CANCEL) {
            m->flags ^= EVOTOP_EXPLAIN;
            EvoTop_PlayVoice(m, 3);
            Flash_GotoLabel(&m->flash[0], "fl_evo_z_ets_out", 1);
            Snd_PlaySe(1, 2);
        }
    }
    if (m->timer == EVOTOP_IDLE_FRAMES) {
        if (m->flags & EVOTOP_EXPLAIN) {
            m->timer = 0;
        } else {
            EvoTop_PlayVoice(m, EVOTOP_LINE_IDLE);
        }
    }
    if (Voice_GetStat(0) == MVOICE_IDLE) {
        if (m->voiceLine == EVOTOP_LINE_IDLE) {
            m->timer = 0;
        }
    }
}

/*
 * After the choice: waits for the save flow to end, starts the fade (20 frames) and a 30-frame timer, fades the
 * voice, the stream sound and (when going back) the music; when the timer runs out stores the cursor in
 * gProgress + 0x7D0 and returns 1.
 */
s32 EvoTop_Leave(EvoTop *m) {
    if (m->flags & EVOTOP_CHOSEN) {
        if (!(m->flags & EVOTOP_LEAVING) && DcSave_IsDone()) {
            if (m->result == 0) {
                Flash_GotoLabel(&m->flash[0], "fl_cancel", 1);
            } else {
                Flash_GotoLabel(&m->flash[0], "fl_out", 1);
            }
            ColorFade_StartOut(0, 0, 0, 20);
            MsgWin_Close();
            IconWin_Close();
            m->timer = 30;
            m->flags |= EVOTOP_LEAVING;
        }
        if (DcSave_IsStarted()) {
            Voice_FadeOutStep(0);
        }
        if (m->flags & EVOTOP_LEAVING) {
            if (ColorFade_IsFadingOut()) {
                Voice_FadeOutStep(0);
                StreamSe_FadeOutStep(0);
                if (m->result == 0) {
                    Bgm_FadeOutStep();
                }
            }
            if (--m->timer == 0) {
                EVO_PROGRESS_CURSOR = m->cursor;
                return 1;
            }
        } else {
            return 0;
        }
    }
    return 0;
}

/* The screen's frame loop. Returns 0 (back), 1 or 2 (the plate chosen). */
s32 EvoTop_Run(s32 section) {
    EvoTop m;

    memset(&m, 0, sizeof(EvoTop));
    EvoTop_Init(&m, section);
    ColorFade_StartIn(0, 0, 0, 20);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        EvoTop_Advance(&m);
        EvoTop_Draw(&m);
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (EvoTop_Leave(&m)) {
            break;
        }
        EvoTop_Input(&m);
    }
    EvoTop_Term(&m);
    Dma_ResetBuffers();
    return m.result;
}
