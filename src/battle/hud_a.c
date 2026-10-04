#include "common.h"
#include "battle/hud_a.h"
#include "battle/battle.h"
#include "sys/heap.h"

/*
 * Battle HUD manager. Source range 0x2187E0-0x219EB0.
 *
 * Owns the seven HUD parts (see battle/hud_a.h), copies the fighters' state into them once per frame
 * (Hud_PreUpdate, from BtlGame_PreUpdate) and updates / draws their node trees (Hud_Draw, from BtlGame_Draw).
 */

typedef f32 HudMtx[4][4];

/* Local view of gCommonRes: the word at +0x28 is the file that holds the five HUD sprite sheets. */
typedef struct HudCommonRes {
    /* 0x00 */ u8 unk0[0x28];
    /* 0x28 */ u32 *hudFile; /* [1], [2], [3], [4], [5]: byte offsets of the sheets */
} HudCommonRes;

extern HudCommonRes *gCommonRes;

extern void *memset(void *dst, s32 c, u32 n);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void Mtx_StoreIdentity(HudMtx m);
extern void Mtx_Translate(HudMtx dst, HudMtx src, f32 *v);
extern void Mtx_RotateZ(HudMtx dst, HudMtx src, f32 angle);
extern void Vu0Cur_LoadIdentity(void);
extern void Vu0Cur_Push(void);
extern void Vu0Cur_Pop(void);
extern void Vu0Cur_ResetStack(void);
extern void Vu0Cur_MulMtx(HudMtx m);
extern void Vu0Cur_MulMtxRev(HudMtx m);
extern s32 BtlSeq_GetTimeLeft(void);
extern s32 func_00212A08(void); /* returns gBtlGameReplayActive */

/* The by-side queries of btl_tech_a.c / btl_capi_b.c. */
extern s32 BtlCtrl_GetWork5A8Count(void);
extern s32 BtlSide_GetHp(s32 side);
extern s32 BtlSide_GetKi(s32 side);
extern s32 BtlSide_GetParamUnk2C(s32 side);
extern s32 BtlSide_TestFlagBE(s32 side);
extern s32 BtlCtrl_GetStatMod0(s32 side);
extern s32 BtlCtrl_GetStatMod1(s32 side);
extern s32 BtlCtrl_GetStatMod2(s32 side);
extern s32 BtlCtrl_GetStatMod3(s32 side);
extern s32 BtlSide_GetBlast(s32 side);
extern s32 BtlSide_GetMaxPower(s32 side);
extern void *BtlCtrl_GetObj(s32 side);
extern s32 BtlSide_IsComboHitNew(s32 side);
extern s32 BtlCtrl_GetClashCount(s32 side);
extern s32 BtlSide_GetComboHits(s32 side);
extern s32 BtlCtrl_GetOppUnkD50(s32 side);
extern s32 BtlSide_GetComboDamage(s32 side);
extern s32 BtlSide_IsComboShown(s32 side);
extern s32 BtlCtrl_GetFlagDEtoE2(s32 side);
extern s32 BtlCtrl_IsFlag6Raised(s32 side);
extern s32 BtlCtrl_TestFlag71(s32 side);
extern s32 BtlCtrl_IsFlag5RaisedInFight(s32 side);
extern s32 BtlCtrl_TestFlagE3(s32 side);
extern s32 BtlCtrl_IsFlag9CRaised(s32 side);
extern s32 BtlCtrl_GetChangePrompt(s32 side, s32 *button, s32 *kind, s32 *padStatus);
extern s32 BtlCtrl_TestFlagE6Pad1(s32 side);
extern s32 BtlCtrl_TestFlagE6Pad(s32 side, s32 *padStatus);
extern s32 BtlCtrl_IsFlag9ERaised(s32 side);
extern s32 BtlCtrl_GetTechPromptPad1(s32 side, s32 *row, s32 *kind, s32 *idx);
extern s32 BtlCtrl_GetSwitchPrompt(s32 side, s32 *buttons, s32 *count, s32 *kind, s32 *padStatus, s32 *idx);
extern s32 BtlCtrl_TestFlagE4(s32 side);
extern s32 BtlCtrl_TestFlagE5(s32 side);
extern s32 BtlSide_CountAlive(s32 side);
extern s32 BtlSide_GetSwitchGauge(s32 side);
extern s32 BtlSide_GetSwitchTargetHp(s32 side);
extern s32 BtlSide_GetSwitchTargetHpMax(s32 side);
extern s32 BtlSide_GetSwitchTarget(s32 side);
extern s32 BtlCtrl_IsSwitching(s32 side);
extern s32 BtlCtrl_TestProgressFrameBit(void);

/* Gauge part (0x21C0E0..0x2224C8 and on). */
extern void func_0021FC10(HudNode **out, HudRes *res); /* init */
extern void func_00222400(void);                       /* term */
extern void func_002224C8(void);                       /* reset */
extern void func_0021F648(s32 side);                   /* select the side to update / draw */
extern void func_00225650(void);
extern void func_0021F700(s32 side, s32 hp);
extern void func_0021F718(s32 side, s32 ki);
extern void func_0021FA20(s32 side, s32 value);
extern void func_0021F8B0(s32 side, s32 mask, s32 up);  /* stat modifier icons: which of 4, and which are raises */
extern void func_0021F810(s32 side, s32 blast);
extern void func_0021F7F8(s32 side, s32 maxPower);
extern void func_0021FAD8(s32 side, void *obj);
extern void func_0021FA38(s32 side, s32 a, s32 b);
extern void func_0021FA88(s32 side, s32 a, s32 b);
extern void func_0021FAF0(f32 seconds);
extern void func_0021FB18(f32 seconds);
extern void func_0021FB40(s32 side, s32 switching);
/* Timer part. */
extern void func_0022F340(HudNode **out, HudRes *res);
extern void func_0022F728(void);
extern void func_0022F948(void);
extern void func_0022F7F0(s32 timeLeft);
extern void func_0022F890(s32 count);
extern void func_0022F2F0(f32 seconds);
extern void func_0022F318(f32 seconds);
/* Notice part. */
extern void func_0022AF88(HudNode **out, HudRes *res);
extern void func_0022B368(void);
extern void func_0022B430(void);
extern void func_0022AF60(s32 on);
extern void func_0022AB50(s32 id);
/* Combo part. */
extern void func_00224198(HudNode **out, HudRes *res);
extern void func_00224908(void);
extern void func_002249D0(void);
extern void func_00223D88(s32 side);                    /* select the side */
extern void func_00223FB8(s32 side, s32 hits);
extern void func_00223DD8(s32 side, s32 damage, s32 isNew);
extern void func_00223DB8(s32 side, s32 value);
extern void func_00223D98(s32 side, s32 message);
extern void func_002240A0(f32 seconds);
extern void func_002240C8(f32 seconds);
extern void func_002240F0(void);
/* Prompt part. */
extern void func_0022E218(HudNode **out, HudRes *res);
extern void func_0022E9A8(void);
extern void func_0022EA70(void);
extern void func_0022D958(s32 side);                    /* select the side */
extern void func_0022DA10(s32 side, s32 button, s32 arg);
extern void func_0022DCB8(s32 side, s32 count, s32 *buttons, s32 *kinds, s32 idx);
extern void func_0022DF30(s32 side);
extern void func_0022E018(s32 side);
extern void func_0022E100(s32 side);
extern void func_0022E0B0(f32 seconds);
extern void func_0022E0D8(f32 seconds);
extern void func_0022E130(void);
extern void func_00225790(void);

/* Clears the mark of every texture entry of the five sheets. */
void Hud_ClearTexMarks(void) {
    s32 i;
    s32 j;

    for (i = 0; i < HUD_RES_COUNT; i++) {
        s32 n = gHud->res[i]->texCount;

        for (j = 0; j < n; j++) {
            HudTex *t = &gHud->res[i]->tex[j];

            t->mark = 0;
        }
    }
}

/* Calls the update callback of a node and of all nodes under it. */
void HudNode_Update(HudNode *node) {
    u32 i;

    if (node->update != NULL) {
        node->update();
    }
    for (i = 0; i < node->childCount; i++) {
        HudNode_Update(node->children[i]);
    }
}

/* Draws a node tree. The node's position and rotation are multiplied into the current VU0 matrix; with `mirror`
   (or HUD_NODE_MIRROR, inherited by the children) the draw callback runs under x' = 511 - x. */
void HudNode_Draw(HudNode *node, s32 mirror) {
    f32 pos[4];
    HudMtx m;
    HudMtx flip;
    u32 i;

    if (node->flags & HUD_NODE_HIDDEN) {
        return;
    }
    Vu0Cur_Push();
    pos[0] = node->x + node->ofsX;
    pos[1] = node->y + node->ofsY;
    pos[2] = 0.0f;
    pos[3] = 1.0f;
    Mtx_StoreIdentity(m);
    Mtx_Translate(m, m, pos);
    Vu0Cur_MulMtx(m);
    Mtx_StoreIdentity(m);
    Mtx_RotateZ(m, m, node->rot);
    Vu0Cur_MulMtx(m);
    if (node->draw != NULL) {
        Vu0Cur_Push();
        if (mirror || (node->flags & HUD_NODE_MIRROR)) {
            Mtx_StoreIdentity(flip);
            flip[0][0] = -1.0f;
            flip[3][0] = 511.0f;
            Vu0Cur_MulMtxRev(flip);
        }
        node->draw(node);
        Vu0Cur_Pop();
    }
    for (i = 0; i < node->childCount; i++) {
        HudNode_Draw(node->children[i], (mirror || (node->flags & HUD_NODE_MIRROR)) ? 1 : 0);
    }
    Vu0Cur_Pop();
}

/* Sets how a replay shows the HUD (see Hud_Draw). */
void Hud_SetReplayMode(s32 mode) {
    gHud->replayMode = mode;
}

/* Shows or hides all five switchable parts. */
void Hud_ShowAll(s32 on) {
    gHud->showGauges = on;
    gHud->showTimer = on;
    gHud->showNotice = on;
    gHud->showCombo = on;
    gHud->showPrompt = on;
}

/* Shows or hides the gauges and the team panel. */
void Hud_ShowGauges(s32 on) {
    gHud->showGauges = on;
}

/* Shows or hides the timer. */
void Hud_ShowTimer(s32 on) {
    gHud->showTimer = on;
}

/* Shows or hides the notice part. */
void Hud_ShowNotice(s32 on) {
    gHud->showNotice = on;
}

/* Shows or hides the combo counter. */
void Hud_ShowCombo(s32 on) {
    gHud->showCombo = on;
}

/* Shows or hides the button prompts. */
void Hud_ShowPrompt(s32 on) {
    gHud->showPrompt = on;
}

/* Battle start: allocates the manager, relocates the five sprite sheets of the common HUD file and builds the
   seven parts. */
void Hud_Init(void) {
    u32 *file;

    gHud = Heap_Alloc(sizeof(Hud), 0x20, 0, 2);
    memset(gHud, 0, sizeof(Hud));
    file = gCommonRes->hudFile;
    gHud->res[0] = (HudRes *)((u8 *)file + ((file[1] >> 2) << 2));
    Res_RelocateOffsets(&gHud->res[0], gHud->res[0], gHud->res[0]);
    gHud->res[3] = (HudRes *)((u8 *)file + ((file[2] >> 2) << 2));
    Res_RelocateOffsets(&gHud->res[3], gHud->res[3], gHud->res[3]);
    gHud->res[1] = (HudRes *)((u8 *)file + ((file[4] >> 2) << 2));
    Res_RelocateOffsets(&gHud->res[1], gHud->res[1], gHud->res[1]);
    gHud->res[2] = (HudRes *)((u8 *)file + ((file[5] >> 2) << 2));
    Res_RelocateOffsets(&gHud->res[2], gHud->res[2], gHud->res[2]);
    gHud->res[4] = (HudRes *)((u8 *)file + ((file[3] >> 2) << 2));
    Res_RelocateOffsets(&gHud->res[4], gHud->res[4], gHud->res[4]);
    func_0021FC10(&gHud->gauge, gHud->res[0]);
    func_0022F340(&gHud->timer, gHud->res[0]);
    HudTeam_Init(&gHud->team, gHud->res[0]);
    func_0022AF88(&gHud->notice, gHud->res[1]);
    func_00224198(&gHud->combo, gHud->res[2]);
    func_0022E218(&gHud->prompt, gHud->res[3]);
    HudCaption_Init(&gHud->caption, gHud->res[4]);
}

/* Battle end: frees the parts and the manager. */
void Hud_Term(void) {
    func_00222400();
    func_0022F728();
    HudTeam_Term();
    func_0022B368();
    func_00224908();
    func_0022E9A8();
    HudCaption_Term();
    Heap_Free(gHud);
}

/* Once per frame, before the fighters update: hands the fighters' state to the parts.
   Per side: health, ki, the +0x2C parameter (0 unless flag 0xBE), the four stat modifiers as two masks (which
   are not zero, which are positive; bit order StatMod0, 3, 1, 2), blast stock, max power, the fighter object;
   the combo counter (new hit: the clash count if there is one, else the combo hits) and combo damage, which
   also shakes the OTHER side's gauge (0x21FA38: 15 / 3 from 9999 damage, 10 / 2 from 1000, else 5 / 2);
   five event messages for the combo part; the button prompt; and the team panel's values.
   Everything that starts an animation is skipped while BATTLE_FLAG_PAUSE is set. */
void Hud_PreUpdate(void) {
    s32 side;

    if (Battle_GetMode() == 3) {
        func_0022F890(BtlCtrl_GetWork5A8Count());
    } else {
        func_0022F7F0(BtlSeq_GetTimeLeft());
    }
    if (func_00212A08()) {
        if (gHud->replayMode != 1) {
            func_0022AF60(0);
        } else {
            func_0022AF60(1);
        }
    } else {
        func_0022AF60(0);
    }
    for (side = 0; side < 2; side++) {
        s32 v;
        s32 i;
        s32 mask;
        s32 up;
        s32 isNew;

        func_0021F700(side, BtlSide_GetHp(side));
        func_0021F718(side, BtlSide_GetKi(side));
        v = BtlSide_GetParamUnk2C(side);
        if (BtlSide_TestFlagBE(side)) {
            func_0021FA20(side, v);
        } else {
            func_0021FA20(side, 0);
        }
        mask = 0;
        up = 0;
        for (i = 0; i < 4; i++) {
            switch (i) {
                case 0:
                    v = BtlCtrl_GetStatMod0(side);
                    break;
                case 1:
                    v = BtlCtrl_GetStatMod3(side);
                    break;
                case 2:
                    v = BtlCtrl_GetStatMod1(side);
                    break;
                case 3:
                    v = BtlCtrl_GetStatMod2(side);
                    break;
                default:
                    continue;
            }
            if (v != 0) {
                mask |= 1 << i;
                if (v > 0) {
                    up |= 1 << i;
                }
            }
        }
        func_0021F8B0(side, mask, up);
        func_0021F810(side, BtlSide_GetBlast(side));
        func_0021F7F8(side, BtlSide_GetMaxPower(side));
        func_0021FAD8(side, BtlCtrl_GetObj(side));
        if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
            isNew = BtlSide_IsComboHitNew(side);
            if (isNew == 1) {
                v = BtlCtrl_GetClashCount(side);
                if (v != 0) {
                    func_00223FB8(side, v);
                } else {
                    func_00223FB8(side, BtlSide_GetComboHits(side));
                }
            }
            if (BtlCtrl_GetOppUnkD50(side)) {
                v = BtlSide_GetComboDamage(side);
                if (v >= 0) {
                    func_00223DD8(side, v, isNew);
                }
                if (v >= 9999) {
                    func_0021FA38(side == 0, 15, 3);
                } else if (v >= 1000) {
                    func_0021FA38(side == 0, 10, 2);
                } else {
                    func_0021FA38(side == 0, 5, 2);
                }
            }
            if (!BtlSide_IsComboShown(side)) {
                func_00223FB8(side, 0);
                func_00223DD8(side, -1, isNew);
            }
        }
        v = BtlCtrl_GetFlagDEtoE2(side);
        if (v >= 0) {
            func_00223DB8(side, v);
        }
        if (BtlCtrl_IsFlag6Raised(side)) {
            func_00223D98(side, 1);
            func_0021FA88(side, 8, 2);
        }
        if (BtlCtrl_TestFlag71(side)) {
            func_00223D98(side, 0);
        }
        if (BtlCtrl_IsFlag5RaisedInFight(side)) {
            func_00223D98(side, 2);
        }
        if (BtlCtrl_TestFlagE3(side)) {
            func_00223D98(side, 3);
        }
        if (BtlCtrl_IsFlag9CRaised(side)) {
            func_00223D98(side, 4);
        }
        /* Single button prompt (flags 0xE7..0xEC through BtlCtrl_GetChangePrompt, else flag 0xE6). The icon
           number is remapped through tbl (identity within each group of four) and moved by kindOfs[kind]
           groups; the third argument is the icon's group. */
        {
            s32 tbl[16] = { 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3 };
            s32 kindOfs[4] = { 0, 0, 1, 2 };
            s32 button;
            s32 kind;
            s32 pad;

            if (BtlCtrl_GetChangePrompt(side, &button, &kind, &pad)) {
                s32 n = button;

                button = tbl[button];
                button += kindOfs[kind] * 4;
                func_0022DA10(side, button, n / 4);
            } else if (BtlCtrl_TestFlagE6Pad1(side)) {
                func_0022DA10(side, 12, 1);
            } else if (BtlCtrl_TestFlagE6Pad(side, &pad)) {
                func_0022DA10(side, 12, pad);
            } else {
                func_0022E100(side);
            }
        }
        /* Technique prompt: a row of icons (buttons[]) with a kind per icon (kinds[]). On pad status 1 it is
           one icon: 5 for kind 3, else the row + 0x47. Otherwise the 1 or 2 button icons are spread to the
           even slots with icon 0x20 (kind 2) between them; icons 0x10..0x13 are moved by 4 * pad status, and
           icons below 0x10 get the kind map[kind]. */
        if (BtlCtrl_IsFlag9ERaised(side)) {
            func_0022E018(side);
        } else {
            s32 buttons[4];
            s32 kinds[4];
            s32 kind;
            s32 idx;
            s32 count;
            s32 pad;

            if (BtlCtrl_GetTechPromptPad1(side, buttons, &kind, &idx)) {
                if (kind == 3) {
                    kinds[0] = kind;
                    buttons[0] = 5;
                } else {
                    kinds[0] = kind;
                    buttons[0] = buttons[0] + 0x47;
                }
                if (kind == 1) {
                    idx = -1;
                }
                if (kind == 3) {
                    idx = -1;
                }
                func_0022DCB8(side, 1, buttons, kinds, idx);
            } else if (BtlCtrl_GetSwitchPrompt(side, buttons, &count, &kind, &pad, &idx)) {
                s32 map[4] = { 3, 2, 1, 0 };

                for (i = count - 1; i >= 0; i--) {
                    buttons[i * 2] = buttons[i];
                }
                for (i = 0; i < count * 2 - 1; i++) {
                    if (i & 1) {
                        buttons[i] = 0x20;
                        kinds[i] = 2;
                    } else if ((u32)(buttons[i] - 0x10) < 4) {
                        buttons[i] = buttons[i] + pad * 4;
                        kinds[i] = 2;
                    } else if ((u32)buttons[i] < 0x10) {
                        kinds[i] = map[kind];
                    } else {
                        kinds[i] = 2;
                    }
                }
                func_0022DCB8(side, count * 2 - 1, buttons, kinds, -1);
            } else if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
                func_0022DF30(side);
            }
        }
        if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
            if (BtlCtrl_TestFlagE4(side)) {
                HudTeam_StartFlipNext(side);
            }
            if (BtlCtrl_TestFlagE5(side)) {
                HudTeam_StartFlipPrev(side);
            }
            v = BtlSide_CountAlive(side);
            HudTeam_SetReserveCount(side, v - (v > 0));
            HudTeam_SetSwitchGauge(side, BtlSide_GetSwitchGauge(side));
            HudTeam_SetTargetHp(side, BtlSide_GetSwitchTargetHp(side));
            HudTeam_SetTargetHpMax(side, BtlSide_GetSwitchTargetHpMax(side));
            HudTeam_SetTarget(side, BtlSide_GetSwitchTarget(side));
            v = BtlCtrl_IsSwitching(side);
            func_0021FB40(side, v);
            HudTeam_SetSwitching(side, v);
        }
    }
    if (BtlCtrl_TestProgressFrameBit()) {
        func_0022AB50(8);
    }
}

/* Round reset: resets the parts and hides them all. */
void Hud_Reset(void) {
    func_002224C8();
    HudTeam_Reset();
    func_002249D0();
    func_0022B430();
    func_0022F948();
    func_0022EA70();
    Hud_ShowAll(0);
}

/* Moves the gauges, team panel, timer, prompts and combo counter off the screen over `seconds`. */
void Hud_SlideOut(f32 seconds) {
    func_0021FAF0(seconds);
    HudTeam_SlideOut(seconds);
    func_0022F2F0(seconds);
    func_0022E0B0(seconds);
    func_002240A0(seconds);
}

/* Brings them back over `seconds`. */
void Hud_SlideIn(f32 seconds) {
    func_0021FB18(seconds);
    HudTeam_SlideIn(seconds);
    func_0022F318(seconds);
    func_0022E0D8(seconds);
    func_002240C8(seconds);
}

/* Updates and draws the parts that are switched on. Mode 7 shows only the caption part. While a replay is played
   gHud->replayMode picks what happens: 0 updates everything and draws only the caption part, 1 draws everything
   but the caption part, 2 only updates. */
void Hud_Draw(void) {
    Vu0Cur_ResetStack();
    Vu0Cur_LoadIdentity();
    func_00225790();
    if (Battle_GetMode() == 7) {
        HudNode_Update(gHud->caption);
        HudNode_Draw(gHud->caption, 0);
    } else if (func_00212A08()) {
        switch (gHud->replayMode) {
            case 0:
                if (gHud->flags & HUD_SHOW_GAUGES) {
                    HudTeam_SelectSide(0);
                    HudNode_Update(gHud->team);
                    HudTeam_SelectSide(1);
                    HudNode_Update(gHud->team);
                }
                if (gHud->flags & HUD_SHOW_GAUGES) {
                    func_00225650();
                    func_0021F648(0);
                    HudNode_Update(gHud->gauge);
                    func_00225650();
                    func_0021F648(1);
                    HudNode_Update(gHud->gauge);
                } else {
                    func_0022AF60(0);
                }
                if (gHud->flags & HUD_SHOW_TIMER) {
                    HudNode_Update(gHud->timer);
                }
                if (gHud->flags & HUD_SHOW_PROMPT) {
                    func_0022D958(0);
                    HudNode_Update(gHud->prompt);
                    if (Battle_IsSplitScreen()) {
                        func_0022D958(1);
                        HudNode_Update(gHud->prompt);
                    }
                }
                if (gHud->flags & HUD_SHOW_COMBO) {
                    func_00223D88(0);
                    HudNode_Update(gHud->combo);
                    func_00223D88(1);
                    HudNode_Update(gHud->combo);
                }
                if (gHud->flags & HUD_SHOW_NOTICE) {
                    HudNode_Update(gHud->notice);
                }
                HudNode_Update(gHud->caption);
                HudNode_Draw(gHud->caption, 0);
                break;
            case 1:
                if (gHud->flags & HUD_SHOW_GAUGES) {
                    HudTeam_SelectSide(0);
                    HudNode_Update(gHud->team);
                    HudNode_Draw(gHud->team, 0);
                    HudTeam_SelectSide(1);
                    HudNode_Update(gHud->team);
                    HudNode_Draw(gHud->team, 0);
                }
                if (gHud->flags & HUD_SHOW_GAUGES) {
                    func_00225650();
                    func_0021F648(0);
                    HudNode_Update(gHud->gauge);
                    HudNode_Draw(gHud->gauge, 0);
                    func_00225650();
                    func_0021F648(1);
                    HudNode_Update(gHud->gauge);
                    HudNode_Draw(gHud->gauge, 0);
                } else {
                    func_0022AF60(0);
                }
                if (gHud->flags & HUD_SHOW_TIMER) {
                    HudNode_Update(gHud->timer);
                    HudNode_Draw(gHud->timer, 0);
                }
                if (gHud->flags & HUD_SHOW_PROMPT) {
                    func_0022D958(0);
                    HudNode_Update(gHud->prompt);
                    HudNode_Draw(gHud->prompt, 0);
                    if (Battle_IsSplitScreen()) {
                        func_0022D958(1);
                        HudNode_Update(gHud->prompt);
                        HudNode_Draw(gHud->prompt, 0);
                    }
                    func_0022E130();
                }
                if (gHud->flags & HUD_SHOW_COMBO) {
                    func_00223D88(0);
                    HudNode_Update(gHud->combo);
                    HudNode_Draw(gHud->combo, 0);
                    func_00223D88(1);
                    HudNode_Update(gHud->combo);
                    HudNode_Draw(gHud->combo, 0);
                    func_002240F0();
                }
                if (gHud->flags & HUD_SHOW_NOTICE) {
                    HudNode_Update(gHud->notice);
                    HudNode_Draw(gHud->notice, 0);
                }
                HudNode_Update(gHud->caption);
                break;
            case 2:
                if (gHud->flags & HUD_SHOW_GAUGES) {
                    HudTeam_SelectSide(0);
                    HudNode_Update(gHud->team);
                    HudTeam_SelectSide(1);
                    HudNode_Update(gHud->team);
                }
                if (gHud->flags & HUD_SHOW_GAUGES) {
                    func_00225650();
                    func_0021F648(0);
                    HudNode_Update(gHud->gauge);
                    func_00225650();
                    func_0021F648(1);
                    HudNode_Update(gHud->gauge);
                } else {
                    func_0022AF60(0);
                }
                if (gHud->flags & HUD_SHOW_TIMER) {
                    HudNode_Update(gHud->timer);
                }
                if (gHud->flags & HUD_SHOW_PROMPT) {
                    func_0022D958(0);
                    HudNode_Update(gHud->prompt);
                    if (Battle_IsSplitScreen()) {
                        func_0022D958(1);
                        HudNode_Update(gHud->prompt);
                    }
                }
                if (gHud->flags & HUD_SHOW_COMBO) {
                    func_00223D88(0);
                    HudNode_Update(gHud->combo);
                    func_00223D88(1);
                    HudNode_Update(gHud->combo);
                }
                if (gHud->flags & HUD_SHOW_NOTICE) {
                    HudNode_Update(gHud->notice);
                }
                HudNode_Update(gHud->caption);
                break;
        }
    } else {
        if (Battle_GetMode() != 1) {
            if (gHud->flags & HUD_SHOW_GAUGES) {
                HudTeam_SelectSide(0);
                HudNode_Update(gHud->team);
                HudNode_Draw(gHud->team, 0);
                HudTeam_SelectSide(1);
                HudNode_Update(gHud->team);
                HudNode_Draw(gHud->team, 0);
            }
        }
        if (gHud->flags & HUD_SHOW_GAUGES) {
            func_00225650();
            func_0021F648(0);
            HudNode_Update(gHud->gauge);
            HudNode_Draw(gHud->gauge, 0);
            func_00225650();
            func_0021F648(1);
            HudNode_Update(gHud->gauge);
            HudNode_Draw(gHud->gauge, 0);
        }
        if (gHud->flags & HUD_SHOW_TIMER) {
            HudNode_Update(gHud->timer);
            HudNode_Draw(gHud->timer, 0);
        }
        if (gHud->flags & HUD_SHOW_PROMPT) {
            func_0022D958(0);
            HudNode_Update(gHud->prompt);
            HudNode_Draw(gHud->prompt, 0);
            if (Battle_IsSplitScreen()) {
                func_0022D958(1);
                HudNode_Update(gHud->prompt);
                HudNode_Draw(gHud->prompt, 0);
            }
            func_0022E130();
        }
        if (gHud->flags & HUD_SHOW_COMBO) {
            func_00223D88(0);
            HudNode_Update(gHud->combo);
            HudNode_Draw(gHud->combo, 0);
            func_00223D88(1);
            HudNode_Update(gHud->combo);
            HudNode_Draw(gHud->combo, 0);
            func_002240F0();
        }
        if (gHud->flags & HUD_SHOW_NOTICE) {
            HudNode_Update(gHud->notice);
            HudNode_Draw(gHud->notice, 0);
        }
    }
    Hud_ClearTexMarks();
}
