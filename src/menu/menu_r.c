#include "common.h"
#include "menu/menu_r.h"
#include "sys/pad.h"

/*
 * SimDay, 0x3840E0..0x3851B0: the last five functions of the board screen of the "sim" ladder (mode 22). The
 * object starts in the previous chunk (src/menu/menu_q_b.c, 0x37F850; work pointer 0x3B7384); its .rodata ends at
 * 0x3B98E4 with the two jump tables of this file (0x3B9890 SimDay_UpdateTalk, 0x3B98B0 SimDay_Input). The strings
 * used here are shared with the head of the object (0x3B9240 "mc_sim_botan%02d", 0x3B9258 "fl_off_start",
 * 0x3B9288 "mc_menu_plate_%d", 0x3B92D0 "fl_on_start"). Append this file to menu_q_b.c:
 * build/scratch_menu_r/merge_test.py does it and all 30 functions of the merged file match.
 */

/*
 * Stand-ins for six functions of the head of the object. SimDay_Input only matches when the compiler has seen
 * their definitions above it (three of its branches come out as the other kind of branch otherwise), which is
 * the evidence that this file and the head are one source file. They emit nothing (ASM_STUB); delete them when
 * the file is appended to the head. SimDay_PickEvent is called with an argument (0 from the board, 1 from the
 * training menu) that the function does not use: in the merged file its definition needs the parameter.
 */
ASM_STUB_BEGIN();
s32 SimDay_CountItems(void) {
    return gSimDay->prevState++;
}
void SimDay_Cmd(s32 cmd) {
    gSimDay->prevState += cmd;
}
void SimDay_PickEvent(s32 unused) {
    gSimDay->prevState += unused;
}
void SimDay_RefreshBoard(void) {
    gSimDay->prevState++;
}
void SimDay_SetupBattle(void) {
    gSimDay->prevState++;
}
void SimDay_ListItems(void) {
    gSimDay->prevState++;
}
ASM_STUB_END();

/* The guide's closing line: starts line `talk` (or the next one when a button cuts the current one short). */
void SimDay_UpdateTalk(void) {
    if (gSimDay->talk[0] == 0) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gPad[0].gamePressed & 0x200) {
        gSimDay->talk[0]++;
        Snd_PlaySe(1, 1);
    } else if (gPad[0].gamePressed & 0x400) {
        gSimDay->talk[0]++;
        Snd_PlaySe(1, 2);
    } else if (gSimDay->msgLine != -1 && Voice_GetStat(0) != SIM_VOICE_IDLE && gSimDay->talk[0] == gSimDay->talk[1]) {
        return;
    }
    switch (gSimDay->talk[0]) {
    case 1:
        gSimDay->msgLine = 0x19;
        break;
    case 2:
        gSimDay->msgLine = 0x1A;
        break;
    case 3:
        gSimDay->msgLine = 0x1B;
        break;
    case 4:
        gSimDay->msgLine = 0x1C;
        break;
    case 5:
        gSimDay->msgLine = 0x1D;
        break;
    }
    Voice_PlayWithSubtitle(NULL, SIM_VOICE_BASE, gSimDay->msgLine);
    gSimDay->talk[0] = 0;
    gSimDay->msgLine = -1;
    gSimDay->talk[1] = gSimDay->talk[0];
}

/*
 * Pad 0: the board's menus, the turn's event script, the window, the announcement of the fight. When the script of
 * the tenth turn of a round is over, SimDay_SetupBattle (menu_q_b.c) writes the battle setup and the two portraits
 * are loaded; confirm then shows the round picture and the screen leaves. Clears *result when the ladder is quit
 * from the window.
 */
void SimDay_Input(s32 *result) {
    s32 ok = gPad[0].gamePressed & 0x200;
    s32 cancel = gPad[0].gamePressed & 0x400;
    s32 up = gPad[0].gameRepeat & 8;
    s32 down = gPad[0].gameRepeat & 4;
    s32 start = gPad[0].gamePressed & 0x1000;

    if (!(gSimDay->flash[SIMDAY_FL_BOARD].flags & MFLASH_PAD)) {
        return;
    }
    if (gSimDay->state == SIMDAY_ST_WINDOW || gSimDay->state == SIMDAY_ST_WINDOW_ITEMS) {
        if (!(gSimDay->flash[SIMDAY_FL_MENU].flags & MFLASH_PAD)) {
            return;
        }
    }
    if (gSimDay->flags & SIMDAY_BUSY) {
        return;
    }
    if (!(gSimDay->flags & SIMDAY_STARTED)) {
        SimDay_RefreshBoard();
        gSimDay->flags |= SIMDAY_STARTED;
    }
    if (gSimDay->state == SIMDAY_ST_BOARD) {
        if (start) {
            SimDay_Cmd(0x14);
            return;
        }
    }
    switch (gSimDay->state) {
    case SIMDAY_ST_BOARD:
        if (up) {
            SimDay_ClipGoto(0, 0, "fl_off_start");
            do {
                gSimDay->cur[gSimDay->state]--;
                if (gSimDay->cur[gSimDay->state] < 0) {
                    gSimDay->cur[gSimDay->state] = 3;
                }
            } while ((SIM_PROG->run.off >> gSimDay->cur[gSimDay->state]) & 1);
            SimDay_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x50;
        } else if (down) {
            SimDay_ClipGoto(0, 0, "fl_off_start");
            do {
                gSimDay->cur[gSimDay->state]++;
                if (gSimDay->cur[gSimDay->state] >= 4) {
                    gSimDay->cur[gSimDay->state] = 0;
                }
            } while ((SIM_PROG->run.off >> gSimDay->cur[gSimDay->state]) & 1);
            SimDay_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x50;
        } else if (ok) {
            if (gSimDay->cur[gSimDay->state] != 0) {
                switch (gSimDay->cur[gSimDay->state]) {
                case 1:
                    SimDay_PickEvent(0);
                    break;
                case 2:
                    SimDay_PickEvent(0);
                    break;
                case 3:
                    SIM_PROG->run.wait = 0x1E;
                    SimDay_PickEvent(0);
                    gSimDay->cur[gSimDay->state] = 0;
                    SimDay_ClipGoto(0, 0, "fl_on_start");
                    break;
                }
                SimDay_Cmd(0xD);
                gSimDay->state = SIMDAY_ST_SCRIPT;
            } else {
                gSimDay->state = SIMDAY_ST_TRAIN;
                SimDay_Cmd(0xE);
                gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x54;
            }
            Snd_PlaySe(1, 1);
        }
        break;
    case SIMDAY_ST_TRAIN:
        if (up) {
            SimDay_ClipGoto(0, 1, "fl_off_start");
            if (--gSimDay->cur[gSimDay->state] < 0) {
                gSimDay->cur[gSimDay->state] = 2;
            }
            SimDay_ClipGoto(0, 1, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x54;
        } else if (down) {
            SimDay_ClipGoto(0, 1, "fl_off_start");
            if (++gSimDay->cur[gSimDay->state] >= 3) {
                gSimDay->cur[gSimDay->state] = 0;
            }
            SimDay_ClipGoto(0, 1, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x54;
        } else if (ok) {
            gSimDay->state = SIMDAY_ST_SCRIPT;
            SimDay_Cmd(0xF);
            SimDay_PickEvent(1);
            Snd_PlaySe(1, 1);
        } else if (cancel) {
            SimDay_Cmd(0x10);
            Snd_PlaySe(1, 2);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x50;
        }
        break;
    case SIMDAY_ST_WINDOW:
        if (up) {
            SimDay_ClipGoto(5, 6, "fl_off_start");
            if (--gSimDay->cur[gSimDay->state] < 0) {
                gSimDay->cur[gSimDay->state] = 2;
            }
            if (gSimDay->cur[gSimDay->state] == 1 && !SimDay_CountItems()) {
                gSimDay->cur[gSimDay->state]--;
            }
            SimDay_ClipGoto(5, 6, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (down) {
            SimDay_ClipGoto(5, 6, "fl_off_start");
            if (++gSimDay->cur[gSimDay->state] >= 3) {
                gSimDay->cur[gSimDay->state] = 0;
            }
            if (gSimDay->cur[gSimDay->state] == 1 && !SimDay_CountItems()) {
                gSimDay->cur[gSimDay->state]++;
            }
            SimDay_ClipGoto(5, 6, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (ok) {
            s32 row = gSimDay->cur[gSimDay->state];

            if (row == 1) {
                if (SimDay_CountItems()) {
                    SimDay_Cmd(0x16);
                    gSimDay->state = SIMDAY_ST_WINDOW_ITEMS;
                    gSimDay->menuText = row;
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
            } else {
                SimDay_Cmd(0x15);
                Snd_PlaySe(1, 1);
            }
        } else if (start) {
            gSimDay->cur[gSimDay->state] = 0;
            SimDay_Cmd(0x15);
        }
        break;
    case SIMDAY_ST_WINDOW_ITEMS:
        if (ok) {
            gSimDay->menuText = 0;
            SimDay_Cmd(0x17);
            gSimDay->state = SIMDAY_ST_WINDOW;
        }
        break;
    case SIMDAY_ST_CONFIRM: {
        s32 answer;

        Dialog_SetMsg(0xE);
        answer = Dialog_Input(0);
        if (answer > 0) {
            gSimDay->state = SIMDAY_ST_QUIT;
            Dialog_SetChoices(0);
            Dialog_Start(1);
        } else if (answer < 0) {
            gSimDay->state = SIMDAY_ST_BACK;
            Dialog_SetChoices(0);
            Dialog_Start(1);
        }
        break;
    }
    case SIMDAY_ST_BACK:
        if (Dialog_IsClosed()) {
            SimDay_Cmd(0x14);
        }
        break;
    case SIMDAY_ST_SELECT:
        if (up) {
            SimDay_ClipGoto(0, 2, "fl_off_start");
            if (--gSimDay->cur[gSimDay->state] < 0) {
                gSimDay->cur[gSimDay->state] = 1;
            }
            SimDay_ClipGoto(0, 2, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (down) {
            SimDay_ClipGoto(0, 2, "fl_off_start");
            if (++gSimDay->cur[gSimDay->state] >= 2) {
                gSimDay->cur[gSimDay->state] = 0;
            }
            SimDay_ClipGoto(0, 2, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (ok) {
            if (gSimDay->cur[gSimDay->state] == 0) {
                gSimDay->unkB8C = 1;
            } else {
                gSimDay->unkB8C = 0;
            }
            SimDay_Cmd(0x12);
            Snd_PlaySe(1, 1);
        }
        break;
    case SIMDAY_ST_SCRIPT:
        if (gSimDay->flags & SIMDAY_WAIT_KEY) {
            if (ok) {
                gSimDay->flags &= ~SIMDAY_WAIT_KEY;
                Snd_PlaySe(1, 1);
            }
        } else if (SimEvent_Run(gSimDay, gSimDay->event)) {
            if (gSimDay->day == SIM_TURNS - 1) {
                gSimDay->state = SIMDAY_ST_VERSUS;
                SimDay_SetupBattle();
                gSimDay->loadState = SIMDAY_LOAD_REQUEST;
            } else {
                gSimDay->state = SIMDAY_ST_BOARD;
            }
            SimDay_ListItems();
            SimDay_Cmd(5);
        }
        break;
    case SIMDAY_ST_VERSUS:
        if (!(gSimDay->flags & SIMDAY_FACES_READY)) {
            return;
        }
        if (ok) {
            Flash_Play(&gSimDay->flash[SIMDAY_FL_ROUND], 1);
            Snd_PlaySe(2, 0x14);
            gSimDay->state = SIMDAY_ST_ROUND;
            Snd_PlaySe(1, 1);
            if (SIM_PROG->run.turn == 0x45) {
                gSimDay->talk[0] = 5;
            } else if (SIM_PROG->chara == 0x66) {
                gSimDay->talk[0] = 4;
            } else {
                gSimDay->talk[0] = Rand_Range(3) + 1;
            }
        }
        break;
    case SIMDAY_ST_ROUND:
        gSimDay->state = SIMDAY_ST_ROUND_WAIT;
        gSimDay->wait = 300;
        break;
    case SIMDAY_ST_ROUND_WAIT:
        gSimDay->wait--;
        if (ok || gSimDay->wait == 0) {
            Bgm_FadeOutStep();
            Voice_StopWithLip();
            gSimDay->flags |= SIMDAY_DONE;
            gSimDay->flags |= SIMDAY_LEAVING;
            gSimDay->timer = 15;
            if (ok) {
                Snd_PlaySe(1, 1);
            }
        }
        break;
    case SIMDAY_ST_QUIT:
        ColorFade_StartOut(0, 0, 0, 0x14);
        *result = 0;
        break;
    }
}

/* Sends the plate under the cursor of state `kind` (0 board, 1 trainings, 2 two-row choice, 6 window) to `label`. */
void SimDay_ClipGoto(s32 movie, s32 kind, char *label) {
    MFlashRef ref;
    char name[0x40];
    MFlash *flash = &gSimDay->flash[movie];

    switch (kind) {
    case 0:
        sprintf(name, "mc_sim_botan%02d", gSimDay->cur[SIMDAY_ST_BOARD]);
        break;
    case 6:
        sprintf(name, "mc_menu_plate_%d", gSimDay->cur[SIMDAY_ST_WINDOW] + 1);
        break;
    case 1:
        sprintf(name, "mc_sim_botan%02d", gSimDay->cur[SIMDAY_ST_TRAIN] + 6);
        break;
    case 2:
        sprintf(name, "mc_sim_botan%02d", gSimDay->cur[SIMDAY_ST_SELECT] + 4);
        break;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Loads the two portraits of the round picture in the background, one step per frame. */
void SimDay_UpdateFaceLoad(void) {
    s32 chara[2] = { 0 };
    MTexRes *res;

    switch (gSimDay->loadState) {
    case SIMDAY_LOAD_REQUEST:
        gSimDay->faceTex[1] = NULL;
        gSimDay->faceTex[0] = NULL;
        chara[0] = SIM_PROG->chara;
        chara[1] = gSimDay->enemyChara;
        File_CancelRequests();
        File_Request(chara[0] + SIM_FACE_FILE, gSimDay->faceFile[0], SIM_FACE_SIZE);
        File_Request(chara[1] + SIM_FACE_FILE, gSimDay->faceFile[1], SIM_FACE_SIZE);
        gSimDay->loadState = SIMDAY_LOAD_READ;
        break;
    case SIMDAY_LOAD_READ:
        if (File_UpdateRequests()) {
            gSimDay->loadState = SIMDAY_LOAD_UNPACK;
        }
        break;
    case SIMDAY_LOAD_UNPACK:
        Sprite_Unpack(gSimDay->faceFile[0], gSimDay->faceRes[0], NULL);
        Sprite_Unpack(gSimDay->faceFile[1], gSimDay->faceRes[1], NULL);
        res = gSimDay->faceRes[0];
        Res_RelocateOffsets(&res, res, res);
        gSimDay->faceTex[1] = res->tex;
        res = gSimDay->faceRes[1];
        Res_RelocateOffsets(&res, res, res);
        gSimDay->faceTex[0] = res->tex;
        gSimDay->flags |= SIMDAY_FACES_READY;
        gSimDay->loadState = SIMDAY_LOAD_IDLE;
        break;
    }
}

/*
 * The board screen (mode 22). Returns 1 when the round's fight was set up (the handler then sets mode 23 and
 * leaves the overlay for the battle), 0 when the ladder was quit from the window (mode 13).
 */
s32 SimDay_Run(s32 section) {
    s32 result = 1;

    SimDay_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        SimDay_UpdateFaceLoad();
        SimDay_Update();
        SimDay_UpdateTalk();
        SimDay_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gSimDay->flags & SIMDAY_GREETED) && (gSimDay->flash[SIMDAY_FL_BOARD].flags & MFLASH_PAD)) {
                gSimDay->flags |= SIMDAY_GREETED;
            }
        }
        if (ColorFade_IsFadingOut()) {
            Bgm_FadeOutStep();
            Voice_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gSimDay->loadState != SIMDAY_LOAD_IDLE) {
                continue;
            }
            break;
        }
        if (gSimDay->flags & SIMDAY_LEAVING) {
            if (gSimDay->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gSimDay->talk[0] == 0) {
            SimDay_Input(&result);
        }
    }
    SIM_PROG->run.off = 0;
    SimDay_Term();
    Dma_ResetBuffers();
    return result;
}
