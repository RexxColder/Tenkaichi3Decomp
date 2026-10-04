#include "common.h"
#include "menu/menu_x.h"
#include "sys/pad.h"
#include "sys/save.h"

/*
 * Menu overlay DBZP.BIN, 0x39FBB8..0x3A3848: head of the Option object, the options screen of mode 62 (the
 * object goes on in the next chunk, menu_y, with the draw function, the plate helper, the reset states and the
 * term function). Its data starts with the work pointer gOption at 0x3BC364 (.data), followed by its strings
 * from 0x3BC370 ("fl_ok" exists a second time at 0x3BC390: a new source file).
 */

#define OPT_RES(n) \
    res = (MTexRes *)MPACK_AT(gOption->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 10) and sets the first state. */
s32 Option_Init(s32 section) {
    MFlashRef ref;
    char name[0x40];
    MTexRes *res = NULL;
    s32 i;

    gOption = Heap_Alloc(sizeof(Option), 0x20, 0, 2);
    memset(gOption, 0, sizeof(Option));
    gOption->pack = (u32 *)MPACK_AT(gMenuArc10, section);
    gOption->res = Sprite_Unpack(gOption->pack, NULL, NULL);
    /*
     * The rest of the function sits in a block that the compiler sees as a loop (`do { } while (0)`, most likely
     * from a macro), and the counter of the loop at the end is cleared here, not in its `for`: the original
     * sets the counter and the first default key (0 and 2, in saved registers) right after the first load of
     * this block, which only this arrangement reproduces.
     */
    do {
        OPT_RES(30);
        i = 0;
        gOption->bg = res;
        OPT_RES(5);
        gOption->tex[22] = MTEX(res, 0);
        OPT_RES(4);
        gOption->tex[27] = MTEX(res, 0);
        OPT_RES(6);
        gOption->tex[21] = MTEX(res, 0);
        gOption->tex[24] = MTEX(res, 1);
        gOption->tex[23] = MTEX(res, 2);
        OPT_RES(1);
        gOption->tex[32] = MTEX(res, 0);
        gOption->tex[31] = MTEX(res, 1);
        OPT_RES(2);
        gOption->tex[33] = MTEX(res, 0);
        OPT_RES(3);
        gOption->tex[25] = MTEX(res, 0);
        gOption->tex[26] = MTEX(res, 1);
        OPT_RES(10);
        gOption->tex[16] = MTEX(res, 0);
        gOption->tex[19] = MTEX(res, 1);
        gOption->tex[18] = MTEX(res, 2);
        OPT_RES(12);
        gOption->tex[15] = MTEX(res, 0);
        OPT_RES(11);
        gOption->tex[17] = MTEX(res, 0);
        gOption->tex[20] = MTEX(res, 1);
        OPT_RES(9);
        gOption->tex[37] = MTEX(res, 0);
        OPT_RES(13);
        gOption->tex[36] = MTEX(res, 0);
        gOption->tex[38] = MTEX(res, 1);
        OPT_RES(21);
        gOption->tex[46] = MTEX(res, 0);
        gOption->tex[47] = MTEX(res, 1);
        gOption->tex[48] = MTEX(res, 2);
        gOption->tex[50] = MTEX(res, 3);
        OPT_RES(22);
        gOption->tex[49] = MTEX(res, 0);
        OPT_RES(23);
        gOption->tex[28] = MTEX(res, 0);
        gOption->tex[29] = MTEX(res, 1);
        gOption->tex[30] = MTEX(res, 3);
        OPT_RES(34);
        gOption->tex[1] = MTEX(res, 0);
        gOption->tex[0] = MTEX(res, 1);
        OPT_RES(17);
        gOption->tex[2] = MTEX(res, 0);
        gOption->tex[5] = MTEX(res, 1);
        gOption->tex[4] = MTEX(res, 2);
        OPT_RES(16);
        gOption->tex[3] = MTEX(res, 0);
        gOption->tex[7] = MTEX(res, 2);
        OPT_RES(15);
        gOption->tex[10] = MTEX(res, 0);
        gOption->tex[13] = MTEX(res, 1);
        gOption->tex[12] = MTEX(res, 2);
        OPT_RES(14);
        gOption->tex[11] = MTEX(res, 0);
        gOption->tex[14] = MTEX(res, 2);
        OPT_RES(19);
        gOption->tex[40] = MTEX(res, 0);
        gOption->tex[39] = MTEX(res, 1);
        gOption->tex[42] = MTEX(res, 6);
        gOption->tex[41] = MTEX(res, 9);
        OPT_RES(25);
        gOption->tex[45] = MTEX(res, 0);
        OPT_RES(24);
        gOption->tex[43] = MTEX(res, 0);
        OPT_RES(26);
        gOption->tex[44] = MTEX(res, 0);
        OPT_RES(20);
        gOption->tex[34] = MTEX(res, 0);
        OPT_RES(27);
        gOption->tex[8] = MTEX(res, 0);
        gOption->tex[9] = MTEX(res, 1);
        OPT_RES(28);
        gOption->tex[35] = MTEX(res, 0);
        OPT_RES(29);
        gOption->tex[6] = MTEX(res, 0);
        gOption->bgmIds = (s32 *)(MPACK_AT(gOption->res, 37) + 0x10);
        gOption->bgmCount = gOption->res[gOption->res[37] >> 2];
        gOption->bgmCount--;
        gOption->bgmCount -= 4;
        BgmList_ApplyUnlocks(&gOption->bgmCount, gOption->bgmIds);
        Flash_Create(&gOption->flash[0], MPACK_AT(gOption->res, 8), gOption->tex);
        Flash_Play(&gOption->flash[0], 1);
        /* original bug: `name` is never written, so this looks up whatever the stack holds (the result is unused) */
        Flash_FindLabel(&gOption->flash[0], NULL, name, &ref);
        Dialog_Init(MPACK_AT(gOption->res, 35), NULL, 0);
        gOption->dialogMsg = MPACK_AT(gOption->res, 38);
        McFlow_Init(1);
        McFlow_SetDoneCb(0, Option_OnSaved, NULL);
        gOption->subtitles = MPACK_AT(gOption->res, 36);
        OPT_RES(18);
        IconWin_Init(MPACK_AT(gOption->res, 32), res);
        IconWin_Open();
        gOption->msgText = MPACK_AT(gOption->res, 33);
        MsgWin_Init(MPACK_AT(gOption->res, 31), gOption->msgText, 0, (s32)gOption->unk1B0);
        MsgWin_Open();
        gOption->cursor = OPT_ITEM_SAVE;
        gOption->state = OPT_ST_START;
        gOption->bgmVolume = gSaveData->bgmVolume;
        gOption->seVolume = gSaveData->seVolume;
        gOption->voiceLine = 0;
        gOption->picker = 0;
        gOption->page = 0;
        gOption->unk134 = 0;
        gOption->bgmBottom = 6;
        gOption->keyOld = -1;
        gOption->keyRow = 5;
        for (; i < OPTION_PAD_NUM; i++) {
            gOption->key[i][0] = 2;
            gOption->key[i][1] = 1;
            gOption->key[i][2] = 0;
            gOption->key[i][3] = 3;
            gOption->key[i][4] = 4;
            gOption->key[i][5] = 5;
            gOption->key[i][6] = 6;
            gOption->key[i][7] = 7;
        }
    } while (0);
    return 1;
}

/* The screen's frame loop; input is skipped while a memory card flow runs. Returns 0. */
s32 Option_Run(s32 section) {
    Option_Init(section);
    ColorFade_StartIn(0, 0, 0, 20);
    while (gOption->mcBusy || Option_Input()) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        Option_Draw();
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();
    }
    Option_Term();
    Dma_ResetBuffers();
    return 0;
}

#define OPT_VOICE(line) Voice_PlayWithSubtitle(gOption->subtitles, OPTION_VOICE_BASE, line)
#define OPT_PLATE(kind, label) Option_PlateGoto(0, kind, label)

/*
 * Pad 0, one state per frame; does nothing until the movie accepts input. Returns 0 when the screen is over
 * (faded out), else 1. Option.state:
 *   -1 start; 0 top page (rows 1..5); 2 / 3 / 4 the screen, sound and controller pages (rows 7..10, 12..16,
 *   17..19); a state equal to a row number means that row is open:
 *   1 save / load picker, 7 three-way type, 8 two-way picker, 9 screen position, 12 stereo / mono, 13 volumes,
 *   14 sound test, 15 voice language, 17 / 21 / 22 key assignment (player, on / off, the keys), 18 / 20
 *   vibration (player, on / off), 10 / 11 / 16 / 19 the reset dialogs (Option_UpdateReset, next chunk),
 *   5 leave (saves first when something changed), -2 that save has ended, 6 fading out.
 * Every setting is written straight into gSaveData; `dirty` only decides whether leaving runs the save flow.
 *
 * Matching notes: the choices inside a state are if / else chains, not switches; the statements that several
 * arms share were merged by the compiler, so they are written out in every arm as the code shape requires
 * (e.g. the busy flag after each McFlow_Start, `valueOld = value` after each inner if of the flag tests).
 */
s32 Option_Input(void) {
    s32 ret = 1;
    s32 i;

    if (!(gOption->flash[0].flags & MFLASH_PAD)) {
        return 1;
    }
    switch (gOption->state) {
    case OPT_ST_START:
        if (gOption->started == 0) {
            OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            gOption->started = 1;
            OPT_VOICE(gOption->voiceLine);
            gOption->state = OPT_ST_TOP;
        }
        break;
    case OPT_ST_TOP:
        if (gOption->greeted == 0) {
            if (Voice_GetStat(0) == MVOICE_IDLE) {
                gOption->voiceLine = 1;
                OPT_VOICE(1);
                gOption->greeted = 1;
            }
        }
        if (gPad[0].gameRepeat & PADG_UP) {
            if (gOption->cursor == OPT_ITEM_EXIT) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
            }
            gOption->cursor--;
            if (gOption->cursor <= 0) {
                gOption->cursor = OPT_ITEM_EXIT;
            }
            if (gOption->cursor == OPT_ITEM_EXIT) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            gOption->voiceLine = gOption->cursor;
            OPT_VOICE(gOption->voiceLine);
            gOption->greeted = 1;
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (gOption->cursor == OPT_ITEM_EXIT) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
            }
            gOption->cursor++;
            if (gOption->cursor > OPT_ITEM_EXIT) {
                gOption->cursor = OPT_ITEM_SAVE;
            }
            if (gOption->cursor == OPT_ITEM_EXIT) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            gOption->voiceLine = gOption->cursor;
            OPT_VOICE(gOption->voiceLine);
            gOption->greeted = 1;
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            /* original quirk: the state is 0 here, so the test is always true (the cursor was probably meant) */
            if (gOption->state != OPT_ITEM_EXIT) {
                OPT_PLATE(OPT_CLIP_ROW, "fl_ok");
            }
            gOption->state = gOption->cursor;
            Snd_PlaySe(1, 1);
            if (gOption->state == OPT_ITEM_SAVE) {
                if (gOption->greeted == 0) {
                    gOption->voiceLine = 1;
                    OPT_VOICE(1);
                    gOption->greeted = 1;
                }
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                gOption->value = 1;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = 0;
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->picker = 0;
            } else if (gOption->state == OPT_ITEM_SCREEN) {
                Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
                gOption->cursor = OPT_ITEM_TYPE;
                gOption->voiceLine = gOption->cursor;
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
                gOption->fromPage = 0;
                gOption->page = 1;
                OPT_VOICE(gOption->cursor);
            } else if (gOption->state == OPT_ITEM_SOUND) {
                Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
                gOption->cursor = OPT_ITEM_STEREO;
                gOption->voiceLine = 11;
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
                gOption->fromPage = 0;
                gOption->page = 2;
                OPT_VOICE(gOption->voiceLine);
            } else if (gOption->state == OPT_ITEM_CTRL) {
                Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
                gOption->cursor = OPT_ITEM_KEYS;
                gOption->voiceLine = 23;
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_on_start");
                gOption->fromPage = 0;
                gOption->page = 3;
                OPT_VOICE(gOption->voiceLine);
            }
            gOption->greeted = 1;
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            if (gOption->cursor == OPT_ITEM_EXIT) {
                if (gOption->dirty != 0) {
                    gOption->unk194 = 1;
                    gOption->state = OPT_ST_SAVED;
                    McFlow_Start(4); /* save, with the prompt used on leaving */
                    gOption->mcBusy = 1;
                    Snd_PlaySe(1, 2);
                    break;
                }
                ColorFade_StartOut(0, 0, 0, 20);
                gOption->state = OPT_ITEM_EXIT;
            } else {
                OPT_PLATE(OPT_CLIP_ROW, "fl_off_start");
                gOption->cursor = OPT_ITEM_EXIT;
                gOption->voiceLine = gOption->cursor;
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            }
            Snd_PlaySe(1, 2);
            OPT_VOICE(gOption->cursor);
        }
        break;
    case OPT_ITEM_SCREEN:
        if (gPad[0].gameRepeat & PADG_UP) {
            if (gOption->cursor == OPT_ITEM_SCR_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_off_start");
            }
            gOption->cursor--;
            if (gOption->cursor < OPT_ITEM_TYPE) {
                gOption->cursor = OPT_ITEM_SCR_RESET;
            }
            if (gOption->cursor == OPT_ITEM_SCR_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            }
            gOption->voiceLine = gOption->cursor;
            Snd_PlaySe(1, 0);
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (gOption->cursor == OPT_ITEM_SCR_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_off_start");
            }
            gOption->cursor++;
            if (gOption->cursor > OPT_ITEM_SCR_RESET) {
                gOption->cursor = OPT_ITEM_TYPE;
            }
            if (gOption->cursor == OPT_ITEM_SCR_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            gOption->voiceLine = gOption->cursor;
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            s32 cur = gOption->cursor;

            if (cur == OPT_ITEM_TYPE) {
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_ok");
                gOption->state = gOption->cursor;
                gOption->type = gSaveData->unk1694;
                gOption->valueOld = gOption->type;
                Flash_GotoLabel(&gOption->flash[0], "fl_type_in", 1);
                Snd_PlaySe(1, 1);
            } else if (cur == OPT_ITEM_SCR2) {
                gOption->value = 0;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = 1;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = gSaveData->unk1698;
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->valueOld = gOption->value;
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_ok");
                gOption->state = gOption->cursor;
                gOption->picker = 4;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                Snd_PlaySe(1, 1);
            } else if (cur == OPT_ITEM_ADJUST) {
                gOption->screenX = gSaveData->screenX;
                gOption->screenY = gSaveData->screenY;
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_ok");
                IconWin_Close();
                MsgWin_Close();
                gOption->state = gOption->cursor;
                Flash_GotoLabel(&gOption->flash[0], "fl_layout_in", 1);
                Snd_PlaySe(1, 1);
            } else if (cur == OPT_ITEM_SCR_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_ok");
                gOption->state = cur;
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_off_start");
            OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            gOption->state = OPT_ST_TOP;
            gOption->cursor = OPT_ITEM_SCREEN;
            gOption->voiceLine = gOption->cursor;
            gOption->page = 0;
            gOption->fromPage = 1;
            OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
            Snd_PlaySe(1, 2);
            OPT_VOICE(gOption->cursor);
        }
        break;
    case OPT_ITEM_TYPE:
        if (gPad[0].gamePressed & PADG_LEFT) {
            gOption->typePrev = gOption->type;
            gOption->type--;
            if (gOption->type < 0) {
                gOption->type = 2;
            }
            Flash_GotoLabel(&gOption->flash[0], "fl_type_right", 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_RIGHT) {
            gOption->typePrev = gOption->type;
            gOption->type++;
            if (gOption->type > 2) {
                gOption->type = 0;
            }
            Flash_GotoLabel(&gOption->flash[0], "fl_type_left", 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            if (gOption->valueOld != gOption->type) {
                gOption->dirty = 1;
            }
            gSaveData->unk1694 = gOption->type;
            gOption->state = OPT_ITEM_SCREEN;
            OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_type_out", 1);
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gOption->state = OPT_ITEM_SCREEN;
            OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_type_out", 1);
            Snd_PlaySe(1, 2);
            OPT_VOICE(gOption->cursor);
        }
        break;
    case OPT_ITEM_ADJUST:
        if ((gPad[0].gameRepeat & PADG_LEFT) && gSaveData->screenX > -16) {
            gSaveData->screenX--;
        } else if ((gPad[0].gameRepeat & PADG_RIGHT) && gSaveData->screenX < 16) {
            gSaveData->screenX++;
        } else if ((gPad[0].gameRepeat & PADG_UP) && gSaveData->screenY > -16) {
            gSaveData->screenY--;
        } else if ((gPad[0].gameRepeat & PADG_DOWN) && gSaveData->screenY < 16) {
            gSaveData->screenY++;
        } else if (gPad[0].gamePressed & PADG_START) {
            gOption->state = OPT_ST_ADJUST_RESET;
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            /* original quirk: only a change of both axes counts as a change */
            if (gSaveData->screenX != gOption->screenX && gSaveData->screenY != gOption->screenY) {
                gOption->dirty = 1;
            }
            OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_layout_out", 1);
            gOption->state = OPT_ITEM_SCREEN;
            IconWin_Open();
            MsgWin_Open();
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            Flash_GotoLabel(&gOption->flash[0], "fl_layout_out", 1);
            gSaveData->screenX = gOption->screenX;
            gSaveData->screenY = gOption->screenY;
            gOption->state = OPT_ITEM_SCREEN;
            MsgWin_Open();
            IconWin_Open();
            OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            Snd_PlaySe(1, 2);
            OPT_VOICE(gOption->cursor);
        }
        break;
    case OPT_ITEM_SOUND:
        if (gPad[0].gameRepeat & PADG_UP) {
            if (gOption->cursor >= OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_off_start");
            }
            gOption->cursor--;
            if (gOption->cursor < OPT_ITEM_STEREO) {
                gOption->cursor = OPT_ITEM_SND_RESET;
            }
            if (gOption->cursor >= OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            if (gOption->cursor == OPT_ITEM_STEREO) {
                gOption->voiceLine = 11;
            } else if (gOption->cursor == OPT_ITEM_VOLUME) {
                gOption->voiceLine = gOption->cursor;
            } else if (gOption->cursor == OPT_ITEM_BGM) {
                gOption->voiceLine = 17;
            } else if (gOption->cursor == OPT_ITEM_SND_RESET) {
                gOption->voiceLine = 10;
            } else {
                gOption->voiceLine = 18;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (gOption->cursor >= OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_off_start");
            }
            gOption->cursor++;
            if (gOption->cursor > OPT_ITEM_SND_RESET) {
                gOption->cursor = OPT_ITEM_STEREO;
            }
            if (gOption->cursor >= OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            if (gOption->cursor == OPT_ITEM_STEREO) {
                gOption->voiceLine = 11;
            } else if (gOption->cursor == OPT_ITEM_VOLUME) {
                gOption->voiceLine = gOption->cursor;
            } else if (gOption->cursor == OPT_ITEM_BGM) {
                gOption->voiceLine = 17;
            } else if (gOption->cursor == OPT_ITEM_SND_RESET) {
                gOption->voiceLine = 10;
            } else {
                gOption->voiceLine = 18;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            if (gOption->cursor == OPT_ITEM_STEREO) {
                gOption->value = 0;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = 1;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_ok");
                gOption->value = gSaveData->soundMode;
                gOption->valueOld = gOption->value;
                gOption->picker = 3;
                gOption->state = gOption->cursor;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                Snd_PlaySe(1, 1);
            } else if (gOption->cursor == OPT_ITEM_VOLUME) {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_ok");
                gOption->value = 0;
                gOption->picker = 7;
                gOption->bgmVolume = gSaveData->bgmVolume;
                gOption->seVolume = gSaveData->seVolume;
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_on_start");
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_off_start");
                gOption->state = gOption->cursor;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                Snd_PlaySe(1, 1);
            } else if (gOption->cursor == OPT_ITEM_BGM) {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_ok");
                MsgWin_Close();
                gOption->state = gOption->cursor;
                Flash_GotoLabel(&gOption->flash[0], "fl_bgm_in", 1);
                OPT_PLATE(OPT_CLIP_BGM, "fl_on_start");
                Bgm_Stop();
                Snd_PlaySe(1, 1);
            } else if (gOption->cursor == OPT_ITEM_VOICE) {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_ok");
                gOption->value = 0;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = 1;
                OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
                gOption->value = (gSaveData->flags ^ 1) & 1;
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->state = gOption->cursor;
                gOption->picker = 6;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                Snd_PlaySe(1, 1);
            } else if (gOption->cursor == OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_ok");
                gOption->state = gOption->cursor;
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            if (gOption->cursor != OPT_ITEM_SND_RESET) {
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            }
            gOption->state = OPT_ST_TOP;
            gOption->cursor = OPT_ITEM_SOUND;
            gOption->voiceLine = gOption->cursor;
            gOption->page = 0;
            gOption->fromPage = 2;
            OPT_VOICE(gOption->voiceLine);
            gOption->fromPage = 3;
            OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    case OPT_ITEM_VOLUME:
        if ((gPad[0].gamePressed & PADG_UP) && gOption->value != 0) {
            gOption->value = 0;
            OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_on_start");
            OPT_PLATE(OPT_CLIP_VOL_SE, "fl_off_start");
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_DOWN) && gOption->value != 1) {
            gOption->value = 1;
            OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_off_start");
            OPT_PLATE(OPT_CLIP_VOL_SE, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_off_start");
            OPT_PLATE(OPT_CLIP_VOL_SE, "fl_off_start");
            OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
            gSaveData->bgmVolume = gOption->bgmVolume;
            gSaveData->seVolume = gOption->seVolume;
            Bgm_SetVolume(0x40);
            gOption->state = OPT_ITEM_SOUND;
            Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
            Snd_PlaySe(1, 2);
            OPT_VOICE(gOption->cursor);
        } else if (gOption->value == 0) {
            if (gPad[0].gameRepeat & PADG_LEFT) {
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_off_start");
                if (--gSaveData->bgmVolume < 0) {
                    gSaveData->bgmVolume = 9;
                }
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_on_start");
                Bgm_SetVolume(0x40);
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gameRepeat & PADG_RIGHT) {
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_off_start");
                if (++gSaveData->bgmVolume > 9) {
                    gSaveData->bgmVolume = 0;
                }
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_on_start");
                Bgm_SetVolume(0x40);
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gamePressed & PADG_CROSS) {
                if (gOption->bgmVolume != gSaveData->bgmVolume) {
                    gOption->dirty = 1;
                }
                gOption->bgmVolume = gSaveData->bgmVolume;
                OPT_PLATE(OPT_CLIP_VOL_BGM, "fl_ok");
                Snd_PlaySe(1, 1);
            }
        } else {
            if (gPad[0].gameRepeat & PADG_LEFT) {
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_off_start");
                if (--gSaveData->seVolume < 0) {
                    gSaveData->seVolume = 9;
                }
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_on_start");
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gameRepeat & PADG_RIGHT) {
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_off_start");
                if (++gSaveData->seVolume > 9) {
                    gSaveData->seVolume = 0;
                }
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_on_start");
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gamePressed & PADG_CROSS) {
                if (gOption->seVolume != gSaveData->seVolume) {
                    gOption->dirty = 1;
                }
                gOption->seVolume = gSaveData->seVolume;
                OPT_PLATE(OPT_CLIP_VOL_SE, "fl_ok");
                Snd_PlaySe(1, 1);
            }
        }
        break;
    case OPT_ITEM_BGM:
        if (gPad[0].gameRepeat & PADG_UP) {
            if (gOption->bgmCursor > 0) {
                OPT_PLATE(OPT_CLIP_BGM, "fl_off_start");
                gOption->bgmCursor--;
                if (gOption->bgmTop > gOption->bgmCursor) {
                    gOption->bgmExtra = gOption->bgmBottom - 1;
                    gOption->bgmTop--;
                    gOption->bgmBottom--;
                    Flash_GotoLabel(&gOption->flash[0], "fl_bgm_down", 1);
                }
                OPT_PLATE(OPT_CLIP_BGM, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (gOption->bgmCursor < gOption->bgmCount - 1) {
                OPT_PLATE(OPT_CLIP_BGM, "fl_off_start");
                gOption->bgmCursor++;
                if (gOption->bgmBottom <= gOption->bgmCursor) {
                    gOption->bgmExtra = gOption->bgmTop;
                    gOption->bgmTop++;
                    gOption->bgmBottom++;
                    Flash_GotoLabel(&gOption->flash[0], "fl_bgm_up", 1);
                }
                OPT_PLATE(OPT_CLIP_BGM, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            OPT_PLATE(OPT_CLIP_BGM, "fl_ok");
            if (gOption->bgmIds[gOption->bgmCursor] != 0x19) {
                Bgm_Play(gOption->bgmIds[gOption->bgmCursor] + 0x10B16);
                Snd_PlaySe(1, 1);
            } else {
                Snd_PlaySe(1, 7);
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            MsgWin_Open();
            gOption->state = OPT_ITEM_SOUND;
            gOption->cursor = OPT_ITEM_BGM;
            Flash_GotoLabel(&gOption->flash[0], "fl_bgm_cansel", 1);
            OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
            Bgm_Play(0x10B18);
            Snd_PlaySe(1, 2);
            OPT_VOICE(17);
        }
        break;
    case OPT_ITEM_CTRL:
        if (gPad[0].gameRepeat & PADG_UP) {
            if (gOption->cursor == OPT_ITEM_CTRL_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_off_start");
            }
            gOption->cursor--;
            if (gOption->cursor < OPT_ITEM_KEYS) {
                gOption->cursor = OPT_ITEM_CTRL_RESET;
            }
            if (gOption->cursor == OPT_ITEM_CTRL_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            if (gOption->cursor == OPT_ITEM_KEYS) {
                gOption->voiceLine = 23;
            } else if (gOption->cursor == OPT_ITEM_VIB) {
                gOption->voiceLine = 24;
            } else {
                gOption->voiceLine = 10;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            if (gOption->cursor == OPT_ITEM_CTRL_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_off_start");
            }
            gOption->cursor++;
            if (gOption->cursor > OPT_ITEM_CTRL_RESET) {
                gOption->cursor = OPT_ITEM_KEYS;
            }
            if (gOption->cursor == OPT_ITEM_CTRL_RESET) {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_on_start");
            } else {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_on_start");
            }
            Snd_PlaySe(1, 0);
            if (gOption->cursor == OPT_ITEM_KEYS) {
                gOption->voiceLine = 23;
            } else if (gOption->cursor == OPT_ITEM_VIB) {
                gOption->voiceLine = 24;
            } else {
                gOption->voiceLine = 10;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            gOption->state = gOption->cursor;
            gOption->value = 0;
            gOption->pad = 0;
            Option_DimPickers();
            if (gOption->state == OPT_ITEM_VIB) {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_ok");
                gOption->ctrlKind = 0;
                gOption->picker = 2;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_in", 1);
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_ok");
                Snd_PlaySe(1, 1);
            } else if (gOption->state == OPT_ITEM_KEYS) {
                OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_ok");
                gOption->ctrlKind = 1;
                gOption->picker = 2;
                Flash_GotoLabel(&gOption->flash[0], "fl_ctrl_in", 1);
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->pad = 0;
                OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_ok");
                Snd_PlaySe(1, 1);
            } else {
                OPT_PLATE(OPT_CLIP_BOTTOM, "fl_ok");
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gOption->fromPage = 4;
            OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_off_start");
            OPT_PLATE(OPT_CLIP_BOTTOM, "fl_off_start");
            gOption->state = OPT_ST_TOP;
            gOption->cursor = OPT_ITEM_CTRL;
            gOption->voiceLine = gOption->cursor;
            OPT_VOICE(gOption->voiceLine);
            gOption->page = 0;
            OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_menu_change", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    case OPT_ITEM_KEYS:
        if ((gPad[0].gamePressed & PADG_LEFT) || (gPad[0].gamePressed & PADG_RIGHT)) {
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_off_start");
            OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
            gOption->pad = gOption->value = gOption->value ^ 1;
            OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_ok");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_DOWN) {
            OPT_PLATE(OPT_CLIP_PICK, "fl_ok");
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_on_start");
            gOption->pad = gOption->value;
            if (gOption->pad == 0) {
                if (gSaveData->flags & SAVE_FLAG_PAD_B(0)) {
                    gOption->value = 1;
                } else {
                    gOption->value = 0;
                }
                gOption->valueOld = gOption->value;
            } else {
                if (gSaveData->flags & SAVE_FLAG_PAD_B(1)) {
                    gOption->value = 1;
                } else {
                    gOption->value = 0;
                }
                gOption->valueOld = gOption->value;
            }
            gOption->state = OPT_ST_KEYS_PAD;
            Snd_PlaySe(1, 1);
            {
                /* one load of the pointer and one store: with gOption written out the two arms reload it */
                Option *o = gOption;

                if (o->value != 0) {
                    o->voiceLine = 31;
                } else {
                    o->voiceLine = 30;
                }
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gOption->state = OPT_ITEM_CTRL;
            gOption->cursor = OPT_ITEM_KEYS;
            gOption->voiceLine = 23;
            OPT_VOICE(gOption->voiceLine);
            OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_on_start");
            Flash_GotoLabel(&gOption->flash[0], "fl_ctrl_out", 1);
            Snd_PlaySe(1, 2);
        }
        break;
    case OPT_ST_KEYS_PAD:
        if (((gPad[0].gamePressed & PADG_LEFT) || (gPad[0].gamePressed & PADG_RIGHT)) && gOption->keyEdit == 0) {
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_off_start");
            gOption->value ^= 1;
            if (gOption->pad == 0) {
                if (gSaveData->flags & SAVE_FLAG_PAD_B(0)) {
                    gSaveData->flags &= ~SAVE_FLAG_PAD_B(0);
                } else {
                    gSaveData->flags |= SAVE_FLAG_PAD_B(0);
                }
            } else {
                if (gSaveData->flags & SAVE_FLAG_PAD_B(1)) {
                    gSaveData->flags &= ~SAVE_FLAG_PAD_B(1);
                } else {
                    gSaveData->flags |= SAVE_FLAG_PAD_B(1);
                }
            }
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_on_start");
            Snd_PlaySe(1, 0);
            if (gOption->value != 0) {
                gOption->voiceLine = 31;
            } else {
                gOption->voiceLine = 30;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_UP) {
            /* original bug: compares the addresses of the two tables, so this is always true */
            if (gSaveData->key[gOption->pad] != gSaveData->keyEdit[gOption->pad]) {
                gOption->dirty = 1;
            }
            if (gOption->value == 0) {
                gSaveData->key[gOption->pad][0] = 2;
                gSaveData->key[gOption->pad][1] = 1;
                gSaveData->key[gOption->pad][2] = 0;
                gSaveData->key[gOption->pad][3] = 3;
                gSaveData->key[gOption->pad][4] = 4;
                gSaveData->key[gOption->pad][5] = 5;
                gSaveData->key[gOption->pad][6] = 6;
                gSaveData->key[gOption->pad][7] = 7;
            } else {
                for (i = 0; i < OPTION_KEY_NUM; i++) {
                    gSaveData->key[0][gOption->pad * 8 + i] = gSaveData->keyEdit[0][gOption->pad * 8 + i];
                }
            }
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_ok");
            gOption->value = gOption->pad;
            gOption->state = OPT_ITEM_KEYS;
            OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
            Snd_PlaySe(1, 2);
        } else if ((gSaveData->flags & SAVE_FLAG_PAD_B(0)) || (gSaveData->flags & SAVE_FLAG_PAD_B(1))) {
            if ((gPad[0].gamePressed & PADG_CROSS) && gOption->value != 0) {
                OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_ok");
                gOption->state = OPT_ST_KEYS_EDIT;
                gOption->keyEdit = 1;
                gOption->keyRow = 3;
                gOption->keyCursor = 2;
                Option_SetKeyMark(gOption->keyRow);
                Snd_PlaySe(1, 1);
            }
        }
        break;
    case OPT_ST_KEYS_EDIT:
        gOption->keyHeld = 0;
        if ((gPad[0].gamePressed & PADG_LEFT) && gOption->keyOld < 0) {
            gOption->keyCursor -= 4;
            if (gOption->keyCursor < 0) {
                gOption->keyCursor += 8;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_RIGHT) && gOption->keyOld < 0) {
            gOption->keyCursor += 4;
            if (gOption->keyCursor > 7) {
                gOption->keyCursor -= 8;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_UP) && gOption->keyOld < 0) {
            if (gOption->keyCursor == 2 || gOption->keyCursor == 6) {
                gOption->keyCursor++;
            } else {
                gOption->keyCursor--;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_DOWN) && gOption->keyOld < 0) {
            if (gOption->keyCursor == 3 || gOption->keyCursor == 7) {
                gOption->keyCursor--;
            } else {
                gOption->keyCursor++;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_TRIANGLE) && gOption->keyOld < 0) {
            /* original bug: address comparisons again, always true */
            if (gOption->key[0] != gSaveData->keyEdit[0] || gOption->key[1] != gSaveData->keyEdit[1]) {
                gOption->dirty = 1;
            }
            gOption->keyEdit = 0;
            gOption->state = OPT_ST_KEYS_PAD;
            OPT_PLATE(OPT_CLIP_KEY_OFF, "fl_on_start");
            Snd_PlaySe(1, 2);
        }
        if (gPad[0].gamePressed != 0) {
            switch (gOption->keyCursor) {
            case 0:
                gOption->keyRow = 5;
                break;
            case 1:
                gOption->keyRow = 4;
                break;
            case 2:
                gOption->keyRow = 3;
                break;
            case 3:
                gOption->keyRow = 2;
                break;
            case 4:
                gOption->keyRow = 7;
                break;
            case 5:
                gOption->keyRow = 6;
                break;
            case 6:
                gOption->keyRow = 0;
                break;
            case 7:
                gOption->keyRow = 1;
                break;
            }
            Option_SetKeyMark(gOption->keyRow);
        }
        if (gPad[0].gameHeld & PADG_CROSS) {
            gOption->keyHeld = 1;
            if (gOption->keyOld < 0) {
                gOption->keyOld = gSaveData->keyEdit[gOption->pad][gOption->keyRow];
            }
        }
        if ((gPad[0].gameHeld & PADG_CROSS) && (gPad[0].gamePressed & PADG_LEFT)) {
            if (--gSaveData->keyEdit[gOption->pad][gOption->keyRow] < 0) {
                gSaveData->keyEdit[gOption->pad][gOption->keyRow] = 3;
            }
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gameHeld & PADG_CROSS) && (gPad[0].gamePressed & PADG_RIGHT)) {
            if (++gSaveData->keyEdit[gOption->pad][gOption->keyRow] > 3) {
                gSaveData->keyEdit[gOption->pad][gOption->keyRow] = 0;
            }
            Snd_PlaySe(1, 0);
        }
        if (gOption->keyHeld == 0) {
            if (gOption->keyOld >= 0) {
                if (gOption->keyOld != gSaveData->keyEdit[gOption->pad][gOption->keyRow]) {
                    for (i = 0; i < OPTION_KEY_NUM; i++) {
                        if (i != gOption->keyRow) {
                            if (gSaveData->keyEdit[gOption->pad][gOption->keyRow] ==
                                gSaveData->keyEdit[gOption->pad][i]) {
                                gSaveData->keyEdit[gOption->pad][i] = gOption->keyOld;
                                break;
                            }
                        }
                    }
                }
                gOption->keyOld = -1;
            }
        }
        break;
    case OPT_ITEM_VIB:
        if ((gPad[0].gamePressed & PADG_LEFT) || (gPad[0].gamePressed & PADG_RIGHT)) {
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_off_start");
            OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
            gOption->pad = gOption->value = gOption->value ^ 1;
            OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_ok");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_DOWN) {
            OPT_PLATE(OPT_CLIP_PICK, "fl_ok");
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_on_start");
            gOption->pad = gOption->value;
            gOption->state = OPT_ST_VIB_EDIT;
            if (gOption->pad == 0) {
                if (gSaveData->flags & SAVE_FLAG_PAD_A(0)) {
                    gOption->value = 1;
                } else {
                    gOption->value = 0;
                }
                gOption->valueOld = gOption->value;
            } else {
                if (gSaveData->flags & SAVE_FLAG_PAD_A(1)) {
                    gOption->value = 1;
                } else {
                    gOption->value = 0;
                }
                gOption->valueOld = gOption->value;
            }
            Snd_PlaySe(1, 1);
            if (gOption->value != 0) {
                gOption->voiceLine = 39;
            } else {
                gOption->voiceLine = 40;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gOption->state = OPT_ITEM_CTRL;
            gOption->cursor = OPT_ITEM_VIB;
            Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
            OPT_PLATE(OPT_CLIP_ROW_CTRL, "fl_on_start");
            gOption->voiceLine = 24;
            OPT_VOICE(gOption->voiceLine);
            Snd_PlaySe(1, 2);
        }
        break;
    case OPT_ST_VIB_EDIT:
        if ((gPad[0].gamePressed & PADG_LEFT) || (gPad[0].gamePressed & PADG_RIGHT)) {
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_off_start");
            gOption->value ^= 1;
            if (gOption->pad == 0) {
                if (gSaveData->flags & SAVE_FLAG_PAD_A(0)) {
                    gSaveData->flags &= ~SAVE_FLAG_PAD_A(0);
                } else {
                    gSaveData->flags |= SAVE_FLAG_PAD_A(0);
                }
            } else {
                if (gSaveData->flags & SAVE_FLAG_PAD_A(1)) {
                    gSaveData->flags &= ~SAVE_FLAG_PAD_A(1);
                } else {
                    gSaveData->flags |= SAVE_FLAG_PAD_A(1);
                }
            }
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_on_start");
            Snd_PlaySe(1, 0);
            if (gOption->value != 0) {
                gOption->voiceLine = 39;
            } else {
                gOption->voiceLine = 40;
            }
            OPT_VOICE(gOption->voiceLine);
        } else if (gPad[0].gamePressed & PADG_UP) {
            if (gOption->valueOld != gOption->value) {
                gOption->dirty = 1;
            }
            OPT_PLATE(OPT_CLIP_VIB_OFF, "fl_ok");
            gOption->state = OPT_ITEM_VIB;
            gOption->value = gOption->pad;
            OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
            Snd_PlaySe(1, 2);
        }
        break;
    case OPT_ITEM_SAVE:
    case OPT_ITEM_SCR2:
    case OPT_ITEM_STEREO:
    case OPT_ITEM_VOICE:
        if ((gPad[0].gamePressed & PADG_LEFT) || (gPad[0].gamePressed & PADG_RIGHT)) {
            OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
            gOption->value ^= 1;
            OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            OPT_PLATE(OPT_CLIP_PICK, "fl_ok");
            Snd_PlaySe(1, 1);
            if (gOption->state == OPT_ITEM_STEREO) {
                if (gOption->valueOld != gOption->value) {
                    gOption->dirty = 1;
                }
                gOption->state = OPT_ITEM_SOUND;
                gSaveData->soundMode = gOption->value;
                SndOpt_Apply();
                Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
            } else if (gOption->state == OPT_ITEM_SCR2) {
                if (gOption->valueOld != gOption->value) {
                    gOption->dirty = 1;
                }
                gSaveData->unk1698 = gOption->value;
                gOption->state = OPT_ITEM_SCREEN;
                gOption->cursor = OPT_ITEM_SCR2;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
            } else if (gOption->state == OPT_ITEM_SAVE) {
                if (gOption->value == 0) {
                    McFlow_Start(0); /* save */
                    gOption->mcBusy = 1;
                } else {
                    McFlow_Start(1); /* load */
                    gOption->mcBusy = 1;
                }
            } else if (gOption->state == OPT_ITEM_VOICE) {
                gOption->dirty = 1;
                gOption->state = OPT_ITEM_SOUND;
                Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
                if (gOption->value != 0) {
                    gSaveData->flags &= ~SAVE_FLAG_VOICE;
                } else {
                    gSaveData->flags |= SAVE_FLAG_VOICE;
                }
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            Snd_PlaySe(1, 2);
            OPT_PLATE(OPT_CLIP_PICK, "fl_off_start");
            if (gOption->state == OPT_ITEM_STEREO) {
                gOption->value = gSaveData->soundMode;
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->state = OPT_ITEM_SOUND;
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
                gOption->voiceLine = 11;
                OPT_VOICE(gOption->voiceLine);
            } else if (gOption->state == OPT_ITEM_SCR2) {
                gOption->value = gSaveData->unk1698;
                OPT_PLATE(OPT_CLIP_PICK, "fl_on_start");
                gOption->state = OPT_ITEM_SCREEN;
                OPT_PLATE(OPT_CLIP_ROW_SCREEN, "fl_on_start");
                OPT_VOICE(gOption->cursor);
            } else if (gOption->state == OPT_ITEM_SAVE) {
                gOption->state = OPT_ST_TOP;
                gOption->cursor = OPT_ITEM_SAVE;
                OPT_PLATE(OPT_CLIP_ROW, "fl_on_start");
                OPT_VOICE(gOption->cursor);
            } else if (gOption->state == OPT_ITEM_VOICE) {
                gOption->state = OPT_ITEM_SOUND;
                gOption->cursor = OPT_ITEM_VOICE;
                OPT_PLATE(OPT_CLIP_ROW_SOUND, "fl_on_start");
                OPT_VOICE(18);
            }
            Flash_GotoLabel(&gOption->flash[0], "fl_value_out", 1);
        }
        break;
    case OPT_ITEM_SCR_RESET:
    case OPT_ST_ADJUST_RESET:
    case OPT_ITEM_SND_RESET:
    case OPT_ITEM_CTRL_RESET:
        Option_UpdateReset();
        break;
    case OPT_ITEM_EXIT:
        if (gOption->dirty != 0) {
            gOption->state = OPT_ST_SAVED;
            McFlow_Start(4);
            gOption->mcBusy = 1;
        } else {
            ColorFade_StartOut(0, 0, 0, 20);
            gOption->state = OPT_ST_FADE;
        }
        break;
    case OPT_ST_SAVED:
        ColorFade_StartOut(0, 0, 0, 20);
        gOption->state = OPT_ST_FADE;
        break;
    case OPT_ST_FADE:
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
        }
        if (ColorFade_IsOutDone()) {
            ret = 0;
        }
        break;
    }
    return ret;
}
