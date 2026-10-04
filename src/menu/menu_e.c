#include "common.h"
#include "menu/menu_e.h"
#include "sys/save.h"

/*
 * CharSel_Run: the frame loop of the one-on-one character / stage / music select (progress modes 38..41 and
 * 44..45), 0x348710..0x348D78. Last function of the screen whose other functions are 0x342190..0x348710; it is
 * here because the chunk was cut in front of it. When the screen ends with a choice, this function writes the
 * battle setup: it is the hand-off from the versus menu to the battle.
 */

extern void CharSel_Init(s32 section);
extern void CharSel_Term(void);
extern void CharSel_Draw(void);
extern void CharSel_Update(void);
extern void CharSel_Input(s32 *result);
extern void CharSel_UpdateFaceLoad(void);
extern void CharSel_UpdateStageLoad(void);

extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                                void *bits);
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                                  void *items);
extern void BattleSetup_Finish(void);
extern s32 CpuLevel_FromSetting(u32 setting);

/* Runs the screen. Returns 0 when it was cancelled, otherwise non-zero with the battle set up. */
s32 CharSel_Run(s32 section) {
    s32 result = 1;
    s32 screenMode;
    s32 bgm;
    s32 timeLimit;
    s32 announcer;
    s32 stage;
    s32 unk10;
    s32 cpuLevel;
    s32 i;

    CharSel_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        CharSel_UpdateFaceLoad();
        CharSel_UpdateStageLoad();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            CharSel_Update();
        }
        CharSel_Draw();
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
            if (gCharSelE->faceState == 4 || gCharSelE->stageState == 4) {
                break;
            }
            continue;
        }
        if (gCharSelE->flags & CHARSEL_LEAVING) {
            if (--gCharSelE->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
                gTsProgress->team[0].member[0] = *gCharSelE->side[0];
                gTsProgress->team[1].member[0] = *gCharSelE->side[1];
                gTsProgress->stageCell = gCharSelE->stage->col + gCharSelE->stage->row * TS_STAGE_COLS;
                gTsProgress->bgm = gCharSelE->bgmIds[gCharSelE->stage->bgm];
            }
        } else {
            CharSel_Input(&result);
        }
    }
    if (result != 0) {
        s32 flag[2] = { 1, 1 };
        s32 mode;

        if (gCharSelE->stage->stage == TS_STAGE_RANDOM) {
            do {
                gCharSelE->stage->stage = gCharSelE->stageIds[Rand_Range(gCharSelE->stageCount - 1)];
            } while (gCharSelE->stage->stage >= TS_STAGE_RANDOM);
        }
        if (gProgress->mode == 0x2D) {
            mode = 6;
            bgm = gCharSelE->bgmIds[gCharSelE->stage->bgm];
            screenMode = 0;
            if (bgm == TS_BGM_RANDOM) {
                bgm = Rand_Range(9) + 8;
            }
            announcer = 1;
            timeLimit = 0;
            unk10 = 0;
            stage = gCharSelE->stage->stage;
            cpuLevel = -1;
            flag[0] = 1;
            flag[1] = 1;
        } else {
            mode = 0;
            bgm = gCharSelE->bgmIds[gCharSelE->stage->bgm];
            screenMode = gCharSelE->players == 1;
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
            stage = gCharSelE->stage->stage;
            unk10 = gSaveData->rule[5] ^ 1;
            announcer = gSaveData->rule[2];
            cpuLevel = CpuLevel_FromSetting(gSaveData->rule[1]);
            flag[0] = gSaveData->rule[3] ^ 1;
            flag[1] = gSaveData->rule[4] ^ 1;
        }
        Battle_ClearWork();
        BattleSetup_SetRule(screenMode, mode, bgm, timeLimit, announcer, stage, unk10);
        switch (gCharSelE->players) {
        case 0:
            BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
            BattleSetup_SetSide(1, 2, 1, 1, flag[1], 1, 0, NULL);
            break;
        case 1:
            BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
            BattleSetup_SetSide(1, 0, 1, 1, 1, 1, 0, NULL);
            break;
        case 2:
            BattleSetup_SetSide(0, 2, 0, 1, flag[0], 1, 0, NULL);
            BattleSetup_SetSide(1, 2, 1, 1, flag[1], 1, 0, NULL);
            break;
        }
        for (i = 0; i < 2; i++) {
            BattleSetup_SetMember(i, 0, gCharSelE->side[i]->chara, gCharSelE->side[i]->color, 0, cpuLevel, 100.0f,
                                  gCharSelE->side[i]->items);
        }
        BattleSetup_Finish();
    }
    CharSel_Term();
    Dma_ResetBuffers();
    return result;
}
