#include "common.h"
#include "menu/menu_f.h"
#include "sys/pad.h"
#include "sys/save.h"

/*
 * TeamSel, tail (menu overlay, 0x34D368..0x351C38): the per-frame update, the pad handler and the main loop
 * of the team select screen; the loop ends by writing the battle setup. The head of the object (helpers,
 * Init, Term, Draw) is src/menu/menu_e_b.c. Called by Duel_Main (0x352CB8, progress modes 38..41).
 */

/* ---- ItemPanel (the next object, src/menu/menu_g.c) ---- */
extern void ItemPanel_Update(s32 side);
extern s32 ItemPanel_Input(s32 side, s32 pad);   /* > 0: item row + 1 whose help was asked for; < 0: closed */
extern void ItemPanel_SetChara(s32 side, s32 chara, s32 slot, s32 set, s32 fromRec);
extern void ItemPanel_Show(s32 side);
extern void ItemPanel_Hide(s32 side);
extern void func_00399730(void);                 /* opens the help window over the item panel */
extern void func_00399760(void);                 /* closes it */

/* ---- main executable ---- */
extern void IconWin_SetIcon(s32 icon);
extern s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);
extern s32 CpuLevel_FromSetting(u32 setting);
extern s32 ChrGrid_MoveRight(TsCell *cells, s32 *col, s32 row);
extern s32 ChrGrid_MoveLeft(TsCell *cells, s32 *col, s32 row);
extern void ChrGrid_MoveDown(TsCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_MoveUp(TsCell *cells, s32 *col, s32 *row, s32 rows);
extern void ChrGrid_NextForm(s32 *forms, s32 *index);
extern void ChrGrid_PrevForm(s32 *forms, s32 *index);
extern s32 ChrGrid_FixCursor(TsCell *cells, s32 *col, s32 *row, s32 rows);
extern void StgGrid_MoveRight(s32 *ids, s32 *col, s32 row);
extern void StgGrid_MoveLeft(s32 *ids, s32 *col, s32 row);
extern void StgGrid_MoveDown(s32 *ids, s32 *col, s32 *row, s32 rows);
extern void StgGrid_MoveUp(s32 *ids, s32 *col, s32 *row, s32 rows);
extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                                void *bits);
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                                  void *items);
extern void BattleSetup_Finish(void);

#ifndef MENU_MENU_E_H
/*
 * TeamSel_Input matches only when the compiler has already seen the DEFINITIONS of three functions of the
 * head of this object: TeamSel_ClipGoto, TeamSel_SetChips and TeamSel_RequestFace (it then fills two delay
 * slots without annulling and gives the loop counter another register). That is evidence that the head and
 * this file are one source file. While this file stands alone, the three placeholders below stand in for the
 * definitions: they are compiled, so the compiler knows the functions, but the assembler skips their output
 * (ASM_STUB_BEGIN / END), so this object still calls the real ones. The bodies only need a side effect (an
 * empty body would make the compiler treat the functions as const). When this file is appended to
 * menu_e_b.c, menu_e.h is already included and this block disappears by itself.
 */
ASM_STUB_BEGIN();
void TeamSel_SetChips(s32 side) {
    gTeamSel->side[side]->flags |= 1;
}
void TeamSel_RequestFace(s32 side) {
    gTeamSel->side[side]->flags |= 1;
}
void TeamSel_ClipGoto(s32 flash, s32 side, s32 kind, char *label) {
    Flash_GotoLabel(&gTeamSel->flash[flash], label, side + kind);
}
ASM_STUB_END();
#endif

/* Advances the movies and the two item panels, and drops the "hidden" marks once the movies say so. */
void TeamSel_Update(void) {
    s32 i;

    for (i = 0; i < TEAMSEL_FLASH_NUM; i++) {
        Flash_Advance(&gTeamSel->flash[i]);
    }
    ItemPanel_Update(0);
    ItemPanel_Update(1);
    for (i = 0; i < TEAMSEL_SIDES; i++) {
        if (gTeamSel->side[i]->mask != 0) {
            if ((&gTeamSel->flash[3])[i].trig & 1) {
                gTeamSel->side[i]->mask = 0;
            }
        }
    }
    if (gTeamSel->stage->mask != 0) {
        if (gTeamSel->flash[0].trig & 1) {
            gTeamSel->stage->mask = 0;
        }
    }
    if (gTeamSel->faceMask != 0) {
        if ((s32)gTeamSel->flash[0].trig < 0) {
            gTeamSel->faceMask = 0;
        }
    }
}

/* The side pad i is choosing for, that side's work, and the member under its cursor. */
#define CUR_N (i + gTeamSel->base)
#define CUR_SIDE (gTeamSel->side[i + gTeamSel->base])
#define CUR_MEMBER (gTeamSel->side[i + gTeamSel->base]->member[gTeamSel->side[i + gTeamSel->base]->cur])
/* The same member's items as one object (TsMember declares them as an array; see TsTeamF). */
#define CUR_ITEMS                                                                                                      \
    (((TsTeamF *)gTeamSel->side[i + gTeamSel->base])->member[gTeamSel->side[i + gTeamSel->base]->cur].items)
#define CUR_CELL                                                                                                        \
    (gTeamSel->cells[i + gTeamSel->base][CUR_MEMBER.row * TS_COLS + CUR_MEMBER.col])
#define CUR_CUSTOM_CELL                                                                                                 \
    (gTeamSel->custom[i + gTeamSel->base][CUR_MEMBER.customCol + CUR_MEMBER.customRow * TS_COLS])

/*
 * The pad handler, once per frame while no fade runs and the screen is not frozen. Does nothing until the
 * background movie accepts input. While TEAMSEL_STAGE is set pad 0 chooses the stage and the music; before
 * that each pad drives its side's state machine (TeamSelSide.state). `*result` is cleared when the screen is
 * left with cancel (TeamSel_Run then returns 0 and writes no battle setup).
 *
 * The return type is a matching device: the original makes no tail calls (so it is not `void`) and treats
 * $v0 as dead at the exit (so it does not return an integer); a function declared `f32` that returns nothing
 * reproduces both. The caller ignores the value.
 */
f32 TeamSel_Input(s32 *result) {
    s32 i;
    s32 pads = 0;
    s32 r;
    s32 chara;
    s32 idx;

    if (!(gTeamSel->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    switch (gTeamSel->players) {
    case TEAMSEL_PLAYERS_VS_CPU:
    case TEAMSEL_PLAYERS_CPU_CPU:
        pads = 1;
        break;
    case TEAMSEL_PLAYERS_TWO:
        pads = 2;
        break;
    }
    if (!(gTeamSel->flags & TEAMSEL_STARTED)) {
        for (i = 0; i < pads; i++) {
            TeamSel_ClipGoto(i + 3, i, TEAMSEL_CLIP_CHIP, "fl_on_start");
        }
        gTeamSel->flags |= TEAMSEL_STARTED;
    }

    if (gTeamSel->flags & TEAMSEL_STAGE) {
        switch (gTeamSel->stage->state) {
        case TEAMSEL_STAGE_GRID:
            if (gPad[0].gameRepeat & 1) {
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveLeft(gTeamSel->stageIds, &gTeamSel->stage->col, gTeamSel->stage->row);
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                if (gTeamSel->stage->col != TS_STAGE_COLS) {
                    gTeamSel->stage->stage =
                        gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + gTeamSel->stage->col];
                    TeamSel_RequestStage();
                }
                Snd_PlaySe(2, 0);
            } else if (gPad[0].gameRepeat & 2) {
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveRight(gTeamSel->stageIds, &gTeamSel->stage->col, gTeamSel->stage->row);
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                if (gTeamSel->stage->col != TS_STAGE_COLS) {
                    gTeamSel->stage->stage =
                        gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + gTeamSel->stage->col];
                    TeamSel_RequestStage();
                }
                Snd_PlaySe(2, 0);
            } else if ((gPad[0].gameRepeat & 8) && gTeamSel->stage->col != TS_STAGE_COLS) {
                Flash_GotoLabel(&gTeamSel->flash[0], "fl_reel_down", 1);
                gTeamSel->stage->mask = 1;
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveUp(gTeamSel->stageIds, &gTeamSel->stage->col, &gTeamSel->stage->row, TS_STAGE_COLS);
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                TeamSel_SetStageChips();
                gTeamSel->stage->stage = gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + gTeamSel->stage->col];
                TeamSel_RequestStage();
                Snd_PlaySe(2, 2);
            } else if ((gPad[0].gameRepeat & 4) && gTeamSel->stage->col != TS_STAGE_COLS) {
                Flash_GotoLabel(&gTeamSel->flash[0], "fl_reel_up", 1);
                gTeamSel->stage->mask = 1;
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_off_start");
                StgGrid_MoveDown(gTeamSel->stageIds, &gTeamSel->stage->col, &gTeamSel->stage->row, TS_STAGE_COLS);
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                TeamSel_SetStageChips();
                gTeamSel->stage->stage = gTeamSel->stageIds[gTeamSel->stage->row * TS_STAGE_COLS + gTeamSel->stage->col];
                TeamSel_RequestStage();
                Snd_PlaySe(2, 1);
            } else if (gPad[0].gamePressed & 0x200) {
                gTeamSel->stage->mask = 0;
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_ok");
                if (gTeamSel->stage->col == TS_STAGE_COLS) {
                    /* the music chip: open the music list */
                    Flash_GotoLabel(&gTeamSel->flash[0], "fl_bgm", 1);
                    IconWin_SetIcon(1);
                    TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_BGM, "fl_on_start");
                    gTeamSel->stage->bgmCursor = gTeamSel->stage->bgm;
                    gTeamSel->stage->state = TEAMSEL_STAGE_BGM;
                } else {
                    /* the stage is chosen: the battle starts after 60 frames */
                    Flash_GotoLabel(&gTeamSel->flash[0], "fl_vs", 1);
                    IconWin_Close();
                    gTeamSel->faceMask = 0;
                    gTeamSel->flags |= TEAMSEL_DECIDED;
                    gTeamSel->flags |= TEAMSEL_LEAVING;
                    gTeamSel->timer = 0x3C;
                }
                Snd_PlaySe(1, 1);
            } else if (gPad[0].gamePressed & 0x400) {
                /* back to the teams */
                gTeamSel->stage->mask = 0;
                gTeamSel->flags ^= TEAMSEL_STAGE;
                Flash_GotoLabel(&gTeamSel->flash[0], "fl_map_cansel", 1);
                IconWin_Close();
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_off_start");
                if (gTeamSel->players != TEAMSEL_PLAYERS_TWO) {
                    Flash_GotoLabel(&gTeamSel->flash[2], "fl_fast_in", 1);
                    TeamSel_ClipGoto(2, 1, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    gTeamSel->side[1]->state = TEAMSEL_ST_TEAM;
                } else {
                    Flash_GotoLabel(&gTeamSel->flash[1], "fl_fast_in", 1);
                    Flash_GotoLabel(&gTeamSel->flash[2], "fl_fast_in", 1);
                    TeamSel_ClipGoto(1, 0, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    TeamSel_ClipGoto(2, 1, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    gTeamSel->side[0]->state = TEAMSEL_ST_TEAM;
                    gTeamSel->side[1]->state = TEAMSEL_ST_TEAM;
                }
                Snd_PlaySe(1, 2);
            }
            break;
        case TEAMSEL_STAGE_BGM:
            if (gPad[0].gameRepeat & 8) {
                gTeamSel->stage->bgmCursor--;
                if (gTeamSel->stage->bgmCursor < 0) {
                    gTeamSel->stage->bgmCursor = gTeamSel->bgmCount - 1;
                }
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gameRepeat & 4) {
                gTeamSel->stage->bgmCursor++;
                if (gTeamSel->stage->bgmCursor > gTeamSel->bgmCount - 1) {
                    gTeamSel->stage->bgmCursor = 0;
                }
                Snd_PlaySe(1, 0);
            } else if (gPad[0].gamePressed & 0x200) {
                if (gTeamSel->bgmIds[gTeamSel->stage->bgmCursor] != TS_BGM_LOCKED) {
                    Flash_GotoLabel(&gTeamSel->flash[0], "fl_bgm_cansel", 1);
                    IconWin_SetIcon(0);
                    TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_BGM, "fl_off_start");
                    TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                    gTeamSel->stage->bgm = gTeamSel->stage->bgmCursor;
                    if (gTeamSel->bgmIds[gTeamSel->stage->bgm] != TS_BGM_RANDOM) {
                        Bgm_Play(gTeamSel->bgmIds[gTeamSel->stage->bgm] + TS_BGM_FILE);
                    }
                    gTeamSel->stage->state = TEAMSEL_STAGE_GRID;
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
            } else if (gPad[0].gamePressed & 0x400) {
                Flash_GotoLabel(&gTeamSel->flash[0], "fl_bgm_cansel", 1);
                IconWin_SetIcon(0);
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_BGM, "fl_off_start");
                TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                gTeamSel->stage->bgmCursor = gTeamSel->stage->bgm;
                gTeamSel->stage->state = TEAMSEL_STAGE_GRID;
                Snd_PlaySe(1, 2);
            }
            break;
        }
    } else {
        for (i = 0; i < pads; i++) {
            /* while the other side's item panel is open this pad is ignored */
            if (gTeamSel->side[(i + gTeamSel->base) ^ 1]->flags & TEAMSEL_SIDE_PANEL) {
                continue;
            }
            switch (CUR_SIDE->state) {
            case TEAMSEL_ST_GRID:
                if (gPad[i].gameRepeat & 1) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                    ChrGrid_MoveLeft(gTeamSel->cells[CUR_N], &CUR_MEMBER.col, CUR_MEMBER.row);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gameRepeat & 2) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                    ChrGrid_MoveRight(gTeamSel->cells[CUR_N], &CUR_MEMBER.col, CUR_MEMBER.row);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gameRepeat & 8) {
                    if (gTeamSel->rows[CUR_N] < 2) {
                        continue;
                    }
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_reel_down", 1);
                    CUR_SIDE->mask = 1;
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                    ChrGrid_MoveUp(gTeamSel->cells[CUR_N], &CUR_MEMBER.col, &CUR_MEMBER.row, gTeamSel->rows[CUR_N]);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                    TeamSel_SetChips(CUR_N);
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 2);
                } else if (gPad[i].gameRepeat & 4) {
                    if (gTeamSel->rows[CUR_N] < 2) {
                        continue;
                    }
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_reel_up", 1);
                    CUR_SIDE->mask = 1;
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                    ChrGrid_MoveDown(gTeamSel->cells[CUR_N], &CUR_MEMBER.col, &CUR_MEMBER.row, gTeamSel->rows[CUR_N]);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                    TeamSel_SetChips(CUR_N);
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 1);
                } else if (gPad[i].gamePressed & 0x200) {
                    if (CUR_CELL.id == TS_RANDOM) {
                        /* random: straight to the plates; the character is drawn when the costume is confirmed */
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_custom_in", 1);
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_ok");
                        gTeamSel->plateCount[CUR_N] = (gTeamSel->battleType == 2) ? 1 : 4;
                        if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                            CUR_MEMBER.plate = 0;
                        }
                        TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                        gTeamSel->colorCount[CUR_N] = 2;
                        if (!(CUR_MEMBER.color < gTeamSel->colorCount[CUR_N])) {
                            CUR_MEMBER.color = 0;
                        }
                        CUR_SIDE->state = TEAMSEL_ST_PLATE;
                        Snd_PlaySe(1, 1);
                    } else if (CUR_CELL.id == TS_CUSTOM) {
                        /* the list of saved custom characters, if there is one */
                        if (gTeamSel->customCount[CUR_N] != 0) {
                            ChrGrid_FixCursor(gTeamSel->custom[CUR_N], &CUR_MEMBER.customCol, &CUR_MEMBER.customRow,
                                              TS_CUSTOM_ROWS);
                            CUR_SIDE->flags |= TEAMSEL_SIDE_CUSTOM;
                            Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_form", 1);
                            CUR_SIDE->mask = 1;
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                            TeamSel_SetCustomChips(CUR_N);
                            CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                            TeamSel_RequestFace(CUR_N);
                            CUR_SIDE->state = TEAMSEL_ST_CUSTOM;
                            Snd_PlaySe(2, 0x29);
                        } else {
                            Snd_PlaySe(1, 7);
                        }
                    } else if (TeamSel_FitsDp(CUR_N, CUR_SIDE->chara)) {
                        if (CUR_CELL.formCount != 0) {
                            CUR_SIDE->flags |= TEAMSEL_SIDE_FORM;
                            if (!(CUR_MEMBER.form < CUR_CELL.formCount)) {
                                CUR_MEMBER.form = 0;
                            }
                            Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_form", 1);
                            CUR_SIDE->mask = 1;
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_on_start");
                            TeamSel_SetGridFormChips(CUR_N);
                            CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.form];
                            TeamSel_RequestFace(CUR_N);
                            CUR_SIDE->state = TEAMSEL_ST_FORM;
                            Snd_PlaySe(2, 0x29);
                        } else if (TeamSel_IsCharaFree(CUR_N, CUR_SIDE->chara)) {
                            Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_custom_in", 1);
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_ok");
                            gTeamSel->plateCount[CUR_N] = (gTeamSel->battleType == 2) ? 1 : 4;
                            if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                                CUR_MEMBER.plate = 0;
                            }
                            TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                            gTeamSel->colorCount[CUR_N] = ChrTbl_WrapCostume(CUR_SIDE->chara, &CUR_MEMBER.color);
                            CUR_SIDE->state = TEAMSEL_ST_PLATE;
                            Snd_PlaySe(1, 1);
                        } else {
                            Snd_PlaySe(1, 7);
                        }
                    } else {
                        Snd_PlaySe(1, 7);
                    }
                } else if (gPad[i].gamePressed & 0x400) {
                    CUR_SIDE->mask = 0;
                    if (CUR_SIDE->memberCount != 0) {
                        /* back to the member list; the member is as it was */
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_out", 1);
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_in", 1);
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                        CUR_MEMBER = gTeamSel->backup[CUR_N];
                        CUR_SIDE->chara = CUR_MEMBER.chara;
                        TeamSel_RequestFace(CUR_N);
                        TeamSel_SumCost(CUR_N, 0);
                        CUR_SIDE->state = TEAMSEL_ST_TEAM;
                    } else if (gTeamSel->players != TEAMSEL_PLAYERS_TWO) {
                        switch (gTeamSel->base) {
                        case 0:
                            /* leave the screen */
                            ColorFade_StartOut(0, 0, 0, 0x14);
                            *result = 0;
                            break;
                        case 1:
                            /* back to the first team */
                            TeamSel_ClipGoto(i + 4, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                            gTeamSel->base = 0;
                            Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_fast_in", 1);
                            TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                            CUR_SIDE->state = TEAMSEL_ST_TEAM;
                            break;
                        }
                    } else {
                        ColorFade_StartOut(0, 0, 0, 0x14);
                        *result = 0;
                    }
                    Snd_PlaySe(1, 2);
                }
                break;

            case TEAMSEL_ST_FORM:
                if (gPad[i].gameRepeat & 1) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_off_start");
                    ChrGrid_PrevForm(CUR_SIDE->chip[0], &CUR_MEMBER.form);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.form];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gameRepeat & 2) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_off_start");
                    ChrGrid_NextForm(CUR_SIDE->chip[0], &CUR_MEMBER.form);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.form];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gamePressed & 0x200) {
                    if (!TeamSel_FitsDp(CUR_N, CUR_SIDE->chara)) {
                        Snd_PlaySe(1, 7);
                    } else if (!TeamSel_IsCharaFree(CUR_N, CUR_SIDE->chara)) {
                        Snd_PlaySe(1, 7);
                    } else {
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_custom_in", 1);
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_ok");
                        if (CUR_SIDE->flags & TEAMSEL_SIDE_CUSTOM) {
                            gTeamSel->plateCount[CUR_N] = (gTeamSel->battleType == 2) ? 1 : 2;
                            if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                                CUR_MEMBER.plate = 0;
                            }
                        } else {
                            gTeamSel->plateCount[CUR_N] = (gTeamSel->battleType == 2) ? 1 : 4;
                            if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                                CUR_MEMBER.plate = 0;
                            }
                        }
                        TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                        gTeamSel->colorCount[CUR_N] = ChrTbl_WrapCostume(CUR_SIDE->chara, &CUR_MEMBER.color);
                        CUR_SIDE->state = TEAMSEL_ST_PLATE;
                        Snd_PlaySe(1, 1);
                    }
                } else if (gPad[i].gamePressed & 0x400) {
                    CUR_SIDE->flags ^= TEAMSEL_SIDE_FORM;
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_form", 1);
                    CUR_SIDE->mask = 1;
                    if (CUR_SIDE->flags & TEAMSEL_SIDE_CUSTOM) {
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_off_start");
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                        TeamSel_SetCustomChips(CUR_N);
                        CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                        TeamSel_RequestFace(CUR_N);
                        CUR_SIDE->state = TEAMSEL_ST_CUSTOM;
                    } else {
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_off_start");
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                        TeamSel_SetChips(CUR_N);
                        CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                        TeamSel_RequestFace(CUR_N);
                        CUR_SIDE->state = TEAMSEL_ST_GRID;
                    }
                    Snd_PlaySe(2, 0x29);
                }
                break;

            case TEAMSEL_ST_PLATE:
                if (gPad[i].gameRepeat & 8) {
                    if (gTeamSel->plateCount[CUR_N] < 2) {
                        continue;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                    CUR_MEMBER.plate--;
                    if (CUR_MEMBER.plate < 0) {
                        CUR_MEMBER.plate = gTeamSel->plateCount[CUR_N] - 1;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gameRepeat & 4) {
                    if (gTeamSel->plateCount[CUR_N] < 2) {
                        continue;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                    CUR_MEMBER.plate++;
                    if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                        CUR_MEMBER.plate = 0;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gamePressed & ((CUR_N == 0) ? 2 : 1)) {
                    /* towards the middle of the screen: look at the item set of the plate */
                    if (CUR_MEMBER.plate <= 0) {
                        continue;
                    }
                    if (CUR_CELL.id != TS_RANDOM) {
                        TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_ok");
                        if (CUR_CELL.id == TS_CUSTOM) {
                            ItemPanel_SetChara(CUR_N, CUR_SIDE->chara, CUR_MEMBER.customCol + CUR_MEMBER.customRow * TS_COLS, 0, 1);
                        } else {
                            ItemPanel_SetChara(CUR_N, CUR_SIDE->chara, CUR_MEMBER.row * TS_COLS + CUR_MEMBER.col,
                                          CUR_MEMBER.plate - 1, 0);
                        }
                        ItemPanel_Show(CUR_N);
                        CUR_SIDE->flags |= TEAMSEL_SIDE_PANEL;
                        CUR_SIDE->state = TEAMSEL_ST_PANEL;
                    }
                } else if (gPad[i].gamePressed & 0x200) {
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_color_in", 1);
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_ok");
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_on_start");
                    CUR_SIDE->state = TEAMSEL_ST_COLOR;
                    Snd_PlaySe(1, 1);
                } else if (gPad[i].gamePressed & 0x400) {
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_custom_cansel", 1);
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                    if (CUR_SIDE->flags & TEAMSEL_SIDE_FORM) {
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_on_start");
                        CUR_SIDE->state = TEAMSEL_ST_FORM;
                    } else if (CUR_SIDE->flags & TEAMSEL_SIDE_CUSTOM) {
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                        CUR_SIDE->state = TEAMSEL_ST_CUSTOM;
                    } else {
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                        CUR_SIDE->state = TEAMSEL_ST_GRID;
                    }
                    Snd_PlaySe(1, 2);
                }
                break;

            case TEAMSEL_ST_PANEL:
                r = ItemPanel_Input(CUR_N, i);
                if (r > 0) {
                    func_00399730();
                    gTeamSel->help = r - 1;
                    CUR_SIDE->state = TEAMSEL_ST_PANEL_HELP;
                } else if (r < 0) {
                    ItemPanel_Hide(CUR_N);
                    CUR_SIDE->flags &= ~TEAMSEL_SIDE_PANEL;
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    CUR_SIDE->state = TEAMSEL_ST_PLATE;
                }
                break;

            case TEAMSEL_ST_PANEL_HELP:
                if (gPad[i].gamePressed & 0x600) {
                    func_00399760();
                    CUR_SIDE->state = TEAMSEL_ST_PANEL;
                    Snd_PlaySe(1, 2);
                }
                break;

            case TEAMSEL_ST_COLOR:
                if (gPad[i].gameRepeat & 1) {
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_off_start");
                    CUR_MEMBER.color--;
                    if (CUR_MEMBER.color < 0) {
                        CUR_MEMBER.color = gTeamSel->colorCount[CUR_N] - 1;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_on_start");
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gameRepeat & 2) {
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_off_start");
                    CUR_MEMBER.color++;
                    if (!(CUR_MEMBER.color < gTeamSel->colorCount[CUR_N])) {
                        CUR_MEMBER.color = 0;
                    }
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_on_start");
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gamePressed & 0x200) {
                    /* the member is complete: fix the character and its item set */
                    CUR_SIDE->mask = 0;
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_color_ok", 1);
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_out", 1);
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_in", 1);
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_ok");
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_off_start");
                    if (CUR_SIDE->flags & TEAMSEL_SIDE_FORM) {
                        CUR_MEMBER.chara = CUR_SIDE->chip[0][CUR_MEMBER.form];
                        if (CUR_MEMBER.plate != 0) {
                            if (CUR_CELL.id == TS_CUSTOM) {
                                CUR_ITEMS = TS_SAVE->rec[CUR_MEMBER.customCol + CUR_MEMBER.customRow * TS_COLS].items;
                            } else {
                                CUR_ITEMS = TS_SAVE->custom[CUR_MEMBER.row * TS_COLS + CUR_MEMBER.col].set[CUR_MEMBER.plate - 1];
                            }
                        } else {
                            memset(&CUR_ITEMS, 0, sizeof(TsItemSet));
                        }
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_off_start");
                    } else if (CUR_SIDE->flags & TEAMSEL_SIDE_CUSTOM) {
                        CUR_MEMBER.chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                        if (CUR_MEMBER.plate != 0) {
                            CUR_ITEMS = TS_SAVE->rec[CUR_MEMBER.customCol + CUR_MEMBER.customRow * TS_COLS].items;
                        } else {
                            memset(&CUR_ITEMS, 0, sizeof(TsItemSet));
                        }
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    } else {
                        if (CUR_CELL.id == TS_RANDOM) {
                            /* draw a character (and one of its forms) until one is not related to another member */
                            do {
                                for (;;) {
                                    idx = Rand_Range(gTeamSel->masterCount[CUR_N]);
                                    if (gTeamSel->cells[CUR_N][idx].id < TS_RANDOM) {
                                        chara = gTeamSel->cells[CUR_N][idx].id;
                                        break;
                                    }
                                }
                                if (gTeamSel->cells[CUR_N][idx].formCount != 0) {
                                    chara = gTeamSel->cells[CUR_N][idx]
                                                .form[Rand_Range(gTeamSel->cells[CUR_N][idx].formCount)];
                                }
                            } while (!TeamSel_IsCharaFree(CUR_N, chara));
                            CUR_MEMBER.chara = chara;
                        } else {
                            CUR_MEMBER.chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                            idx = CUR_MEMBER.col + CUR_MEMBER.row * TS_COLS;
                        }
                        if (CUR_MEMBER.plate != 0) {
                            CUR_ITEMS = TS_SAVE->custom[idx].set[CUR_MEMBER.plate - 1];
                        } else {
                            memset(&CUR_ITEMS, 0, sizeof(TsItemSet));
                        }
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_off_start");
                    }
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_off_start");
                    if (CUR_SIDE->cur == CUR_SIDE->memberCount) {
                        /* it was a new member: the cursor goes to the next free slot */
                        CUR_SIDE->memberCount = CUR_SIDE->cur + 1;
                        CUR_SIDE->cur = CUR_SIDE->memberCount;
                    }
                    TeamSel_SetTeamTex(CUR_N);
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    if (CUR_SIDE->cur == TS_CUR_MENU) {
                        CUR_SIDE->chara = CUR_SIDE->member[0].chara;
                        TeamSel_RequestFace(CUR_N);
                    }
                    if (CUR_SIDE->flags & TEAMSEL_SIDE_FORM) {
                        CUR_SIDE->flags ^= TEAMSEL_SIDE_FORM;
                    }
                    if (CUR_SIDE->flags & TEAMSEL_SIDE_CUSTOM) {
                        CUR_SIDE->flags ^= TEAMSEL_SIDE_CUSTOM;
                    }
                    CUR_SIDE->state = TEAMSEL_ST_TEAM;
                    Snd_PlaySe(1, 1);
                } else if (gPad[i].gamePressed & 0x400) {
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_color_cansel", 1);
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_COLOR_PLATE, "fl_off_start");
                    TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                    CUR_SIDE->state = TEAMSEL_ST_PLATE;
                    Snd_PlaySe(1, 2);
                }
                break;

            case TEAMSEL_ST_TEAM:
                if (gPad[i].gameRepeat & 1) {
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_off_start");
                    CUR_SIDE->cur--;
                    if (CUR_SIDE->cur < 0) {
                        CUR_SIDE->cur = TS_CUR_MENU;
                    } else if (CUR_SIDE->cur > CUR_SIDE->memberCount) {
                        CUR_SIDE->cur = CUR_SIDE->memberCount;
                    }
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    if (CUR_SIDE->cur == TS_CUR_MENU) {
                        CUR_SIDE->chara = CUR_SIDE->member[0].chara;
                    } else {
                        CUR_SIDE->chara = CUR_MEMBER.chara;
                    }
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gameRepeat & 2) {
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_off_start");
                    CUR_SIDE->cur++;
                    if (CUR_SIDE->cur > TS_CUR_MENU) {
                        CUR_SIDE->cur = 0;
                    } else if (CUR_SIDE->cur > CUR_SIDE->memberCount) {
                        CUR_SIDE->cur = TS_CUR_MENU;
                    }
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    if (CUR_SIDE->cur == TS_CUR_MENU) {
                        CUR_SIDE->chara = CUR_SIDE->member[0].chara;
                    } else {
                        CUR_SIDE->chara = CUR_MEMBER.chara;
                    }
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(1, 0);
                } else if (gPad[i].gamePressed & 0x200) {
                    if (CUR_SIDE->cur != TS_CUR_MENU) {
                        /* choose (or change) the member under the cursor */
                        TeamSel_SumCost(CUR_N, 1);
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_out", 1);
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_in", 1);
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_ok");
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                        TeamSel_SetChips(CUR_N);
                        CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                        TeamSel_RequestFace(CUR_N);
                        gTeamSel->backup[CUR_N] = CUR_MEMBER;
                        CUR_SIDE->state = TEAMSEL_ST_GRID;
                        Snd_PlaySe(1, 1);
                    } else if (CUR_SIDE->memberCount != 0) {
                        /* the team is complete */
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_out", 1);
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_ok");
                        CUR_SIDE->state = TEAMSEL_ST_DONE;
                        if (gTeamSel->players != TEAMSEL_PLAYERS_TWO && gTeamSel->base == 0) {
                            /* one pad: it goes on to the second team */
                            gTeamSel->base = 1;
                            TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                            CUR_SIDE->state = TEAMSEL_ST_GRID;
                        }
                        Snd_PlaySe(1, 1);
                    } else {
                        Snd_PlaySe(1, 7);
                    }
                } else if (gPad[i].gamePressed & 0x400) {
                    /* remove a member */
                    if (CUR_SIDE->memberCount == 1) {
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_out", 1);
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_in", 1);
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_ok");
                        CUR_SIDE->cur = 0;
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                        TeamSel_SetChips(CUR_N);
                        CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                        TeamSel_RequestFace(CUR_N);
                        CUR_SIDE->state = TEAMSEL_ST_GRID;
                    } else if (CUR_SIDE->cur == TS_CUR_MENU) {
                        TeamSel_RemoveMember(CUR_N, CUR_SIDE->memberCount - 1);
                    } else if (CUR_SIDE->cur == CUR_SIDE->memberCount) {
                        TeamSel_RemoveMember(CUR_N, CUR_SIDE->cur - 1);
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_off_start");
                        CUR_SIDE->cur--;
                        TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                        CUR_SIDE->chara = CUR_MEMBER.chara;
                        TeamSel_RequestFace(CUR_N);
                    } else {
                        TeamSel_RemoveMember(CUR_N, CUR_SIDE->cur);
                        CUR_SIDE->chara = CUR_MEMBER.chara;
                        TeamSel_RequestFace(CUR_N);
                    }
                    CUR_SIDE->memberCount--;
                    TeamSel_SetTeamTex(CUR_N);
                    Snd_PlaySe(1, 2);
                }
                break;

            case TEAMSEL_ST_CUSTOM:
                if (gPad[i].gameRepeat & 1) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    ChrGrid_MoveLeft(gTeamSel->custom[CUR_N], &CUR_MEMBER.customCol, CUR_MEMBER.customRow);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gameRepeat & 2) {
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    ChrGrid_MoveRight(gTeamSel->custom[CUR_N], &CUR_MEMBER.customCol, CUR_MEMBER.customRow);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 0);
                } else if (gPad[i].gameRepeat & 8) {
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_reel_down", 1);
                    CUR_SIDE->mask = 1;
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    ChrGrid_MoveUp(gTeamSel->custom[CUR_N], &CUR_MEMBER.customCol, &CUR_MEMBER.customRow, TS_CUSTOM_ROWS);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                    TeamSel_SetCustomChips(CUR_N);
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 2);
                } else if (gPad[i].gameRepeat & 4) {
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_reel_up", 1);
                    CUR_SIDE->mask = 1;
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    ChrGrid_MoveDown(gTeamSel->custom[CUR_N], &CUR_MEMBER.customCol, &CUR_MEMBER.customRow, TS_CUSTOM_ROWS);
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_on_start");
                    TeamSel_SetCustomChips(CUR_N);
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.customCol];
                    TeamSel_RequestFace(CUR_N);
                    Snd_PlaySe(2, 1);
                } else if (gPad[i].gamePressed & 0x200) {
                    if (!TeamSel_FitsDp(CUR_N, CUR_SIDE->chara)) {
                        Snd_PlaySe(1, 7);
                    } else if (CUR_CUSTOM_CELL.formCount != 0) {
                        CUR_SIDE->flags |= TEAMSEL_SIDE_FORM;
                        if (!(CUR_MEMBER.form < CUR_CUSTOM_CELL.formCount)) {
                            CUR_MEMBER.form = 0;
                        }
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_form", 1);
                        CUR_SIDE->mask = 1;
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_FORM_CHIP, "fl_on_start");
                        TeamSel_SetFormChips(CUR_N);
                        CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.form];
                        TeamSel_RequestFace(CUR_N);
                        CUR_SIDE->state = TEAMSEL_ST_FORM;
                        Snd_PlaySe(2, 0x29);
                    } else if (TeamSel_IsCharaFree(CUR_N, CUR_SIDE->chara)) {
                        Flash_GotoLabel(&gTeamSel->flash[CUR_N + 5], "fl_custom_in", 1);
                        TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_ok");
                        gTeamSel->plateCount[CUR_N] = (gTeamSel->battleType == 2) ? 1 : 2;
                        if (!(CUR_MEMBER.plate < gTeamSel->plateCount[CUR_N])) {
                            CUR_MEMBER.plate = 0;
                        }
                        TeamSel_ClipGoto(CUR_N + 5, CUR_N, TEAMSEL_CLIP_CUSTOM_PLATE, "fl_on_start");
                        gTeamSel->colorCount[CUR_N] = ChrTbl_WrapCostume(CUR_SIDE->chara, &CUR_MEMBER.color);
                        CUR_SIDE->state = TEAMSEL_ST_PLATE;
                        Snd_PlaySe(1, 1);
                    } else {
                        Snd_PlaySe(1, 7);
                    }
                } else if (gPad[i].gamePressed & 0x400) {
                    CUR_SIDE->flags ^= TEAMSEL_SIDE_CUSTOM;
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 3], "fl_form", 1);
                    CUR_SIDE->mask = 1;
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CUSTOM_CHIP, "fl_off_start");
                    TeamSel_ClipGoto(CUR_N + 3, CUR_N, TEAMSEL_CLIP_CHIP, "fl_on_start");
                    TeamSel_SetChips(CUR_N);
                    CUR_SIDE->chara = CUR_SIDE->chip[0][CUR_MEMBER.col];
                    TeamSel_RequestFace(CUR_N);
                    CUR_SIDE->state = TEAMSEL_ST_GRID;
                    Snd_PlaySe(2, 0x29);
                }
                break;

            case TEAMSEL_ST_DONE:
                if (gTeamSel->flags & TEAMSEL_STAGE) {
                    continue;
                }
                if (gTeamSel->side[(i + gTeamSel->base) ^ 1]->state == TEAMSEL_ST_DONE) {
                    /* both teams are complete: on to the stage */
                    Flash_GotoLabel(&gTeamSel->flash[0], "fl_map", 1);
                    IconWin_Open();
                    TeamSel_ClipGoto(0, 0, TEAMSEL_CLIP_STAGE_CHIP, "fl_on_start");
                    gTeamSel->flags |= TEAMSEL_STAGE;
                    gTeamSel->faceMask = 1;
                    gTeamSel->stage->state = TEAMSEL_STAGE_GRID;
                } else if (gPad[i].gamePressed & 0x400) {
                    /* two pads: reopen this side's team while the other is still choosing */
                    Flash_GotoLabel(&gTeamSel->flash[CUR_N + 1], "fl_fast_in", 1);
                    TeamSel_ClipGoto(CUR_N + 1, CUR_N, TEAMSEL_CLIP_TEAM, "fl_on_start");
                    CUR_SIDE->state = TEAMSEL_ST_TEAM;
                    Snd_PlaySe(1, 2);
                }
                break;
            }
        }
    }
}

/*
 * The team select screen: its own frame loop until the fade out is done and both picture loaders are idle.
 * Returns 0 when the screen was left with cancel; otherwise the battle setup has been written (mode 0,
 * the versus battle) and 1 is returned.
 */
s32 TeamSel_Run(s32 section) {
    s32 result = 1;
    s32 i;
    s32 j;
    s32 bgm;
    s32 timeLimit;
    s32 cpuLevel;

    TeamSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        TeamSel_UpdateFaceLoad();
        TeamSel_UpdateStageLoad();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            TeamSel_Update();
        }
        TeamSel_Draw();
        Font_FlushAll();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsFadingOut()) {
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gTeamSel->faceState == TEAMSEL_LOAD_IDLE || gTeamSel->stageState == TEAMSEL_LOAD_IDLE) {
                break;
            }
            continue;
        }
        if (gTeamSel->flags & TEAMSEL_LEAVING) {
            if (--gTeamSel->timer == -1) {
                /* the stage was chosen 60 frames ago: fade out and remember the choices for the next visit */
                ColorFade_StartOut(0, 0, 0, 0x14);
                gTsProgress->team[0] = *(TsTeam *)gTeamSel->side[0];
                gTsProgress->team[1] = *(TsTeam *)gTeamSel->side[1];
                gTsProgress->stageCell = gTeamSel->stage->col + gTeamSel->stage->row * TS_STAGE_COLS;
                gTsProgress->bgm = gTeamSel->bgmIds[gTeamSel->stage->bgm];
            }
            continue;
        }
        TeamSel_Input(&result);
    }

    if (result != 0) {
        s32 handicap[2] = { 1, 1 };

        if (gTeamSel->stage->stage == TS_STAGE_RANDOM) {
            do {
                gTeamSel->stage->stage = gTeamSel->stageIds[Rand_Range(gTeamSel->stageCount - 1)];
            } while (gTeamSel->stage->stage >= TS_STAGE_RANDOM);
        }
        bgm = gTeamSel->bgmIds[gTeamSel->stage->bgm];
        if (bgm == TS_BGM_RANDOM) {
            bgm = Rand_Range(9) + 8;
        }
        switch (gSaveData->rule[0]) {
        case 0:
            timeLimit = 1;
            break;
        case 1:
            timeLimit = 2;
            break;
        case 2:
            timeLimit = 3;
            break;
        case 3:
            timeLimit = 4;
            break;
        case 4:
            timeLimit = 0;
            break;
        default:
            timeLimit = 5;
            break;
        }
        handicap[0] = gSaveData->rule[3] ^ 1;
        handicap[1] = gSaveData->rule[4] ^ 1;
        cpuLevel = CpuLevel_FromSetting(gSaveData->rule[1]);
        Battle_ClearWork();
        BattleSetup_SetRule(gTeamSel->players == TEAMSEL_PLAYERS_TWO, 0, bgm, timeLimit, gSaveData->rule[2],
                            gTeamSel->stage->stage, gSaveData->rule[5] ^ 1);
        switch (gTeamSel->players) {
        case TEAMSEL_PLAYERS_VS_CPU:
            BattleSetup_SetSide(0, 0, 0, gTeamSel->side[0]->memberCount, 1, 1, 0, NULL);
            BattleSetup_SetSide(1, 2, 1, gTeamSel->side[1]->memberCount, handicap[1], 1, 0, NULL);
            break;
        case TEAMSEL_PLAYERS_TWO:
            BattleSetup_SetSide(0, 0, 0, gTeamSel->side[0]->memberCount, 1, 1, 0, NULL);
            BattleSetup_SetSide(1, 0, 1, gTeamSel->side[1]->memberCount, 1, 1, 0, NULL);
            break;
        case TEAMSEL_PLAYERS_CPU_CPU:
            BattleSetup_SetSide(0, 2, 0, gTeamSel->side[0]->memberCount, handicap[0], 1, 0, NULL);
            BattleSetup_SetSide(1, 2, 1, gTeamSel->side[1]->memberCount, handicap[1], 1, 0, NULL);
            break;
        }
        for (i = 0; i < TEAMSEL_SIDES; i++) {
            for (j = 0; j < gTeamSel->side[i]->memberCount; j++) {
                BattleSetup_SetMember(i, j, gTeamSel->side[i]->member[j].chara, gTeamSel->side[i]->member[j].color,
                                      0, cpuLevel, 100.0f, gTeamSel->side[i]->member[j].items);
            }
        }
        BattleSetup_Finish();
    }
    TeamSel_Term();
    Dma_ResetBuffers();
    return result;
}
