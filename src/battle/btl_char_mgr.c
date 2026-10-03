#include "common.h"
#include "battle/battle.h"
#include "battle/battle_setup.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_scene.h"
#include "battle/btl_seq.h"
#include "sys/common.h"
#include "sys/heap.h"
#include "sys/save.h"

/*
 * Fighter manager, 0x1C0058..0x1C2FF0: the roster (gBtlChars), the per-fighter reset and the whole-roster
 * phases of a battle frame. Layouts: include/battle/btl_char_mgr.h.
 *
 * Lifecycle
 *   BtlChar_AllocAll(n)   Battle_Init, n = 2: the manager, n fighters of 0x1600 bytes and two side arrays, all
 *                         zeroed; caches six table pointers of common file 2; BtlChar_ResetAll(); func_001D9F90().
 *   BtlChar_ResetAll()    Battle_Restart, and BtlChars_CheckStart when the fight starts: releases what the
 *                         fighters hold, zeroes the manager's counters and every fighter, then per fighter
 *                         side / pad / index, BtlChar_BindObject, BtlChar_Reset.
 *   BtlChar_FreeAll()     Battle_Term.
 *
 * One frame (the callers are Battle_Loop and Battle_Update, src/battle/battle.c). "per fighter" means a loop
 * over the roster in index order, and every per-fighter body does nothing while the fighter is frozen
 * (chr->freeze > 0, BtlChar_IsFrozen).
 *
 *   1. BtlChars_CheckStart      before Pad_Update, paused or not. The first frame the sequence is in Ready or
 *                               Fight: BtlChar_ResetAll() (except in mode 1) and set BTL_CHARS_STARTED.
 *   2. BtlChars_SampleInput     skipped under PAUSE / LOADING. per fighter: stage 2, BtlInput_Sample.
 *   3. BtlChars_UpdateInput     skipped under PAUSE; under LOADING only BtlChars_ClearObjFlag2.
 *        frame counter          if started and func_001D63A8() == 0: frame++, unk1C counts down
 *        BtlChars_CheckRoundReset
 *        BtlChars_UpdateFreeze  decides who is frozen this frame
 *        func_001DA058          empties the manager's sound request list at +0xA0
 *        per fighter            BtlChar_BeginFrame: stage 3, previous-frame copies, per-frame clears,
 *                               BtlInput_Update, re-reads of member bonus / ability
 *        func_001D8128(0)       snapshot 0 of every fighter's position
 *        func_001DAE98          two-fighter test that sets held flag 0xBA on both
 *        per fighter            BtlChar_UpdateSeqFlags: held flags 1..4 from the sequence state, 0x135
 *   4. BtlChars_UpdateMain      skipped under PAUSE / LOADING.
 *        per fighter            BtlChar_UpdateMotion: func_001DCB58 (the action state machine: func_001E23D0
 *                               sets stages 4 and 5 itself), seven placement requests (func_001D75C8..), then
 *                               pose -> object, animation (func_0024C958, func_0024CC88), matrices
 *                               (func_0024E3F8), object -> pose
 *        per fighter            BtlChar_UpdateStage6: stage 6, func_001DBCE0, func_001D35B0
 *        func_001D7C30          fighter against fighter push-out
 *        func_001D8128(1)
 *        per fighter            BtlChar_UpdateStage7: stage 7, func_001DF4A8, func_001DF210, matrices
 *        func_001D8128(2)
 *        per fighter            BtlChar_UpdateStage8: stage 8, func_001DEF70 (stage queries), matrices,
 *                               func_001B21A8 / func_001B15B8 on the object
 *        func_001D8128(3)
 *        per fighter            BtlChar_UpdateStage9: stage 9, func_001CD558, matrices
 *        func_001D8128(4)
 *        per fighter            BtlChar_UpdateCamera: ChrCam_StartCut, ChrCam_UpdateDemo, ChrCam_UpdateInput,
 *                               then the object's final placement, func_001D7088, matrices, func_001D2D30
 *        BtlReplay_UpdateViewer, func_001D3110, func_001CF220 (gauges), BtlChars_UpdateCollision (hits)
 *        per fighter            BtlChar_UpdateStage10: stage 10, func_001D3668, func_001D2E88
 *        func_001D9900
 *      (Battle_Update then runs the effect scene: BtlScene_Update, func_001AF9C0, BtlScene_PostUpdate)
 *   5. BtlChars_PostScene       skipped under PAUSE / LOADING. per fighter: BtlChar_PostScene (ChrCam_Update,
 *                               low-health state, the vector pushed onto the opponent).
 *      (Battle_Update: BtlScene_CheckStageChange, loader polls, stage, both cameras, BtlCam_UpdateOverride)
 *   6. BtlChars_EndFrame        skipped under LOADING. func_001D6178() runs even when paused; the rest is
 *                               skipped under PAUSE:
 *        per fighter            BtlChar_UpdateLate
 *        BtlChars_UpdateObjFlag2
 *        func_001DA098          plays the sound request list at +0xA0
 *        per fighter            BtlChar_EndFrame: stage 11, BtlChar_RaiseEvents, BtlChar_UpdateVibration,
 *                               stage 12
 *        func_001D8128(5)
 *
 * What the callees named func_XXXXXXXX do is from a first read of their code, not from matching C:
 *   func_001DA970(chr, n) sets the stage byte chr+0x1084 = n and moves the flag bits queued "for stage n"
 *                        from the pending arrays into the live ones (func_001DA8B8)
 *   func_001DACE8(chr, f) flag f went 0 -> 1 this frame;   func_001DAD40(chr, f) went 1 -> 0
 *   func_001DAFB0(chr, m) chr->frameBits |= m
 *   func_001E0358(chr)   chr->action;  func_001E0430(chr) a class number of the action
 *   func_001CE030(chr, n) &chr->members[n]; func_001CE050(chr) the current member; func_001CE1B8(chr) its gauge
 *   func_001CE468(chr)   chr->curMember; func_001CE3D0(chr, n) sets it; func_001CE3D8(chr) members with health
 *   func_001CEE90(chr)   health / healthMax of the current member; func_001CEED8(chr) the same over the team
 *   func_001CF0C8(chr, n) the current member has ability bit n
 *   func_001CDD40(chr, member, init, variant, health)  loads a member's parameters
 *   func_001CEB40(chr, n) adds n health, clamped to the maximum
 *   func_001DB770(chr)   the opponent's roster index (1 for side 0, else 0)
 *   func_001D63A8()      gBtlChars + 0x274: non-zero while time is stopped
 *   func_001D7198(chr, f) writes the pose (position, rotation matrix) into the BtlObj
 *   func_001D70E8(chr)   reads position / rotation back from the BtlObj into the pose
 *   func_0024C958, func_0024CC88, func_0024E3F8, func_0024DD80, func_0024DDF0, func_0024FE78, func_0024FFE8,
 *   func_00250888, func_00250CB8, func_00250CD0, func_00250D38   BtlObj (model / skeleton) updates
 *   func_001D8128(n)     per fighter func_001D8088(chr, &pose, &pose + 0x10, n): position snapshot n
 *   func_0020E280(chr)   byte 0xAD of the object's parameter block (BtlObj + 0x91C)
 *   func_0020C9F0(chr)   id of the technique in use
 *   func_00121E20(v)     zeroes a Vec4
 * BtlChar_GetObj / BtlChar_GetPos / BtlChar_IsFrozen / BtlUtil_Clamp / BtlUtil_Max and the ChrCam_ / BtlReplay_
 * names come from config/symbols (btl_replay.txt, btl_char_cam.txt); they are declared here with this
 * module's own view of the fighter.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern s32 BtlChar_GetCount(void);
extern BtlMgrChr *BtlChar_Get(s32 idx);
extern void BtlChar_SetFlag(BtlMgrChr *chr, s32 flag);
extern s32 BtlChar_TestFlag(BtlMgrChr *chr, s32 flag);
extern void BtlChar_SetHeldFlag(BtlMgrChr *chr, s32 flag);
extern void BtlChar_ClearFlag(BtlMgrChr *chr, s32 flag);
extern void BtlInput_Init(BtlMgrChr *chr);
extern void BtlInput_Sample(BtlMgrChr *chr);
extern void BtlInput_Update(BtlMgrChr *chr);
extern void BtlInput_Stub(BtlMgrChr *chr);
extern s32 BtlInput_IsPressed(BtlMgrChr *chr, u32 mask);
extern s32 BtlCtrl_CanAct(s32 side);

extern void func_00121E20(void *vec);
extern void func_001AF740(void);
extern void func_001B15B8(BtlMgrObj *obj);
extern void func_001B21A8(BtlMgrObj *obj, s32 arg1, s32 arg2);
extern void func_001C31A0(BtlMgrChr *chr);
extern void func_001C31D0(BtlMgrChr *chr);
extern void func_001C33D8(BtlMgrChr *chr, s32 idx, s32 val, s32 add);
extern void func_001C3E60(BtlMgrChr *chr, s32 arg1, f32 arg2);
extern void func_001C42A8(BtlMgrChr *chr);
extern void func_001C4580(BtlMgrChr *chr, f32 rate);
extern void func_001C45A8(BtlMgrChr *chr, f32 rate);
extern s32 func_001C4638(BtlMgrChr *chr);
extern s32 func_001C4650(s32 idx);
extern void ChrCam_UpdateInput(BtlMgrChr *chr);
extern void ChrCam_Update(BtlMgrChr *chr);
extern void ChrCam_UpdateDemo(BtlMgrChr *chr);
extern void ChrCam_StartCut(BtlMgrChr *chr);
extern void func_001CA520(void);
extern void func_001CA618(BtlMgrChr *chr);
extern void func_001CD558(BtlMgrChr *chr);
extern void func_001CD9C8(BtlMgrChr *chr);
extern void func_001CDD40(BtlMgrChr *chr, s32 member, s32 init, s32 variant, f32 health);
extern BtlMgrMember *func_001CE030(BtlMgrChr *chr, s32 idx);
extern BtlMgrMember *func_001CE050(BtlMgrChr *chr);
extern BtlMgrGauge *func_001CE1B8(BtlMgrChr *chr);
extern void func_001CE3D0(BtlMgrChr *chr, s32 member);
extern s32 func_001CE3D8(BtlMgrChr *chr);
extern s32 func_001CE468(BtlMgrChr *chr);
extern s32 func_001CE470(BtlMgrChr *chr);
extern void func_001CEB40(BtlMgrChr *chr, s32 amount);
extern f32 func_001CEE90(BtlMgrChr *chr);
extern f32 func_001CEED8(BtlMgrChr *chr);
extern s32 func_001CF0C8(BtlMgrChr *chr, s32 ability);
extern void func_001CF220(void);
extern void func_001D2D10(BtlMgrChr *chr);
extern void func_001D2D30(BtlMgrChr *chr);
extern void func_001D2E88(BtlMgrChr *chr);
extern void func_001D2EC8(BtlMgrChr *chr);
extern void func_001D2EF0(BtlMgrChr *chr);
extern void func_001D3110(void);
extern void func_001D3568(BtlMgrChr *chr);
extern void func_001D35B0(BtlMgrChr *chr);
extern void func_001D3668(BtlMgrChr *chr);
extern void func_001D6150(void);
extern void func_001D6178(void);
extern s32 func_001D63A8(void);
extern void func_001D6438(BtlMgrChr *chr);
extern void func_001D64A0(BtlMgrChr *chr);
extern void func_001D6578(BtlMgrChr *chr, s32 arg1);
extern void func_001D7088(BtlMgrChr *chr);
extern void func_001D70E8(BtlMgrChr *chr);
extern void func_001D7198(BtlMgrChr *chr, s32 arg1);
extern void func_001D73B0(BtlMgrChr *chr);
extern void func_001D7570(BtlMgrChr *chr);
extern void func_001D75C8(BtlMgrChr *chr);
extern void func_001D7628(BtlMgrChr *chr);
extern void func_001D7688(BtlMgrChr *chr);
extern void func_001D7720(BtlMgrChr *chr);
extern void func_001D7908(BtlMgrChr *chr);
extern void func_001D79A0(BtlMgrChr *chr);
extern void func_001D7A48(BtlMgrChr *chr);
extern void func_001D7C30(void);
extern void func_001D8080(BtlMgrChr *chr);
extern void func_001D8128(s32 pass);
extern void func_001D8270(BtlMgrChr *chr, void *vec, s32 arg2, s32 arg3);
extern void func_001D82E0(BtlMgrChr *chr, void *vec, s32 arg2);
extern void BtlReplay_Rewind(BtlMgrChr *chr);
extern void BtlReplay_ResetViewer(void);
extern void BtlReplay_UpdateViewer(void);
extern void func_001D9900(void);
extern void func_001D9F90(void);
extern void func_001DA058(void);
extern void func_001DA098(void);
extern void func_001DA5A8(BtlMgrChr *chr);
extern void func_001DA970(BtlMgrChr *chr, s32 stage);
extern void func_001DAAF0(BtlMgrChr *chr, s32 first, s32 last);
extern s32 func_001DACE8(BtlMgrChr *chr, s32 flag);
extern s32 func_001DAD40(BtlMgrChr *chr, s32 flag);
extern void func_001DAD98(BtlMgrChr *chr);
extern void func_001DAE98(void);
extern void func_001DAFB0(BtlMgrChr *chr, s32 bits);
extern s32 func_001DB770(BtlMgrChr *chr);
extern void func_001DBCE0(BtlMgrChr *chr);
extern s32 BtlUtil_Clamp(s32 val, s32 lo, s32 hi);
extern s32 BtlUtil_Max(s32 a, s32 b);
extern BtlMgrObj *BtlChar_GetObj(BtlMgrChr *chr);
extern BtlMgrPose *BtlChar_GetPos(BtlMgrChr *chr);
extern s32 BtlChar_IsFrozen(BtlMgrChr *chr);
extern s32 BtlChar_TestMemberUnk70(BtlMgrChr *chr);
extern void BtlChar_UpdateVibration(BtlMgrChr *chr);
extern void BtlChar_StopVoiceOnFlag(BtlMgrChr *chr);
extern void BtlChar_TickVoiceTimers(BtlMgrChr *chr);
extern void func_001DC9A0(BtlMgrChr *chr);
extern void func_001DCB58(BtlMgrChr *chr);
extern void func_001DEF70(BtlMgrChr *chr);
extern void func_001DF210(BtlMgrChr *chr);
extern void func_001DF4A8(BtlMgrChr *chr);
extern void func_001E0310(BtlMgrChr *chr);
extern s32 func_001E0358(BtlMgrChr *chr);
extern s32 func_001E0430(BtlMgrChr *chr);
extern s32 func_00206B70(s32 objId);
extern s32 func_00206CC0(s32 objId);
extern s32 func_00206E88(s32 objId);
extern s32 func_0020C9F0(BtlMgrChr *chr);
extern s32 func_0020E108(BtlMgrChr *chr);
extern s32 func_0020E280(BtlMgrChr *chr);
extern s32 func_0020E480(BtlMgrChr *chr);
extern s32 func_0020F188(BtlMgrChr *chr);
extern s32 func_0020F1B0(BtlMgrChr *chr);
extern void func_0024C958(BtlMgrObj *obj);
extern void func_0024CC88(BtlMgrObj *obj);
extern void func_0024DD80(BtlMgrObj *obj, void *vec);
extern void func_0024DDF0(BtlMgrObj *obj);
extern void func_0024E3F8(BtlMgrObj *obj);
extern void func_0024FE78(BtlMgrObj *obj, s32 arg1, s32 arg2);
extern void func_0024FFE8(BtlMgrObj *obj);
extern void func_00250888(BtlMgrObj *obj, s32 arg1);
extern void func_00250CB8(BtlMgrObj *obj, void *vec);
extern void func_00250CD0(BtlMgrObj *obj);
extern void func_00250D38(BtlMgrObj *obj);

/* The word of the object's +0x1660 block that mirrors BtlChar_TestMemberUnk70(). */
#define OBJ_WORD_18028(obj) (*(s32 *)((obj)->unk1660 + 0x18028))

/* A table inside a data file: the header word gives its byte offset. */
#define FILE_TABLE(file, word) ((u32 *)(file) + ((u32 *)(file))[word] / 4)

/* Binds the fighter to its side's object and clears the state that belongs to the old object. */
void BtlChar_BindObject(BtlMgrChr *chr) {
    BtlMgrObj *obj;
    s32 i;

    chr->objId = BattleSide_GetObjId(chr->side);
    obj = BtlChar_GetObj(chr);
    chr->unk980 = -1;
    for (i = 0; i < 3; i++) {
        if (obj->file[i] != NULL) {
            chr->objTbl[i] = FILE_TABLE(obj->file[i], 3);
        }
    }
    BtlChar_ClearFlag(chr, 0xBE);
    func_001DAAF0(chr, 0x98, 0x99);
    func_00250CD0(obj);
    memset(chr->unkE00, 0, 0x40);
    func_001C31D0(chr);
    OBJ_WORD_18028(obj) = 0;
}

/* Copies one member's seven bonus levels from the battle setup. */
void BtlChar_CopyMemberBonus(BtlMgrChr *chr, s32 idx) {
    BtlMgrMember *dst = &chr->members[idx];
    BattleMember *src = BattleSide_GetMember(chr->side, idx);

    dst->bonus[0] = src->bonus[1];
    dst->bonus[1] = src->bonus[2];
    dst->bonus[2] = src->bonus[3];
    dst->bonus[3] = src->bonus[4];
    dst->bonus[4] = src->bonus[5];
    dst->bonus[5] = src->bonus[6];
    dst->bonus[6] = src->bonus[7];
}

/* Copies every member's bonus levels. */
void BtlChar_CopyAllBonus(BtlMgrChr *chr) {
    s32 i;

    for (i = 0; i < chr->memberCount; i++) {
        BtlChar_CopyMemberBonus(chr, i);
    }
}

/* Copies one member's four ability words from the battle setup. */
void BtlChar_CopyMemberAbility(BtlMgrChr *chr, s32 idx) {
    BtlMgrMember *dst = &chr->members[idx];
    BattleMember *src = BattleSide_GetMember(chr->side, idx);
    s32 i;

    for (i = 0; i < 4; i++) {
        dst->ability[i] = src->ability[i];
    }
}

/* Copies every member's ability words. */
void BtlChar_CopyAllAbility(BtlMgrChr *chr) {
    s32 i;

    for (i = 0; i < chr->memberCount; i++) {
        BtlChar_CopyMemberAbility(chr, i);
    }
}

/* Resets one fighter from its side of the battle setup. */
void BtlChar_Reset(BtlMgrChr *chr) {
    s32 side = chr->side;
    s32 i;
    BtlMgrObj *obj;
    BattleMember *m;
    BtlMgrMember *mem;
    BattleResult *res;

    func_001DA970(chr, BTL_CHR_STAGE_RESET);
    chr->optA = BattleSide_GetOptionA(side);
    chr->optBOff = BattleSide_GetOptionB(side) == 0;
    chr->padFlagA = (gSaveData->flags & (2 << chr->pad)) != 0;
    chr->unk1590 = -1;
    chr->unk1594 = -1;
    BtlInput_Init(chr);
    func_001E0310(chr);
    chr->injectOn = BattleSide_IsCpu(side);
    func_001D7570(chr);
    BtlReplay_Rewind(chr);
    func_001D6438(chr);
    BtlChar_SetHeldFlag(chr, 5);
    obj = BtlChar_GetObj(chr);
    func_001D7198(chr, 1);
    func_001C3E60(chr, 0, 0.0f);
    func_0024C958(obj);
    func_0024E3F8(obj);
    chr->memberCount = BattleSide_GetMemberCount(side);
    chr->unk1300 = BattleSide_GetUnk1FC(side);
    chr->unkCF4 = BattleSide_GetUnk200(side);
    BtlChar_CopyAllBonus(chr);
    BtlChar_CopyAllAbility(chr);
    for (i = 0; i < chr->memberCount; i++) {
        mem = &chr->members[i];
        mem->present = 1;
        mem->chara = BattleSide_GetMemberChara(side, i);
        mem->costume = BattleSide_GetMemberCostume(side, i);
        mem->cpuLevel = BattleSide_GetMember(side, i)->cpuLevel;
        mem->aiType = BattleSide_GetMember(side, i)->aiType;
        m = BattleSide_GetMember(side, i);
        func_001CDD40(chr, i, 1, BattleSide_GetMemberVariant(side, i), m->health);
    }
    chr->unk99C = 0;
    if (func_001CF0C8(chr, 0x1B)) {
        chr->unk99C = 100000;
    }
    res = BattleResult_GetPtr();
    res->unk1C[chr->side] = 0;
    res->unk24[chr->side] = 0;
    if (func_001CE1B8(chr)->unk20 != 0) {
        BtlChar_GetObj(chr)->flags |= 0x40000000;
    }
}

/* Applies the form change of the current action once the new model is loaded. */
void BtlChar_OnModelLoaded(BtlMgrChr *chr) {
    BtlMgrObj *obj;
    BtlMgrMember *m;
    BtlMgrGauge *g;
    BtlMgrMember *other;
    BtlMgrGauge *og;
    f32 ratio;
    s32 i;

    BtlChar_BindObject(chr);
    obj = BtlChar_GetObj(chr);
    BtlChar_SetFlag(chr, 0x12B);
    func_001C42A8(chr);
    func_0024C958(obj);
    func_001D7198(chr, 1);
    func_0024E3F8(obj);
    switch (func_001E0358(chr)) {
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF:
    case 0xF0:
        /* transformation: keep the health ratio */
        m = func_001CE050(chr);
        m->chara = chr->newChara;
        g = &m->gauge;
        m->costume = chr->newCostume;
        g->unk20 = chr->new20;
        ratio = func_001CEE90(chr);
        func_001CDD40(chr, func_001CE468(chr), 0, 0, 0.0f);
        g->health = (f32)g->healthMax * ratio + 0.5f;
        if (func_0020E280(chr) & 1) {
            g->health += 5000;
        }
        if (func_0020E280(chr) & 2) {
            g->health += 10000;
        }
        if (func_0020E280(chr) & 4) {
            g->ki = g->kiMax;
        }
        g->health = BtlUtil_Clamp(g->health, 1, g->healthMax);
        g->blast = BtlUtil_Clamp(g->blast, 0, g->blastMax);
        break;
    case 0xF3:
    case 0xF4:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
        /* switch to another team member */
        func_001CE3D0(chr, chr->nextMember);
        func_001CE470(chr);
        chr->unk99C = 0;
        g = &func_001CE050(chr)->gauge;
        if (func_001CF0C8(chr, 0x43)) {
            BtlChar_SetHeldFlag(chr, 6);
            g->unk1C = 30000;
            g->ki = g->kiMax;
        } else {
            BtlChar_ClearFlag(chr, 6);
            g->unk1C = 0;
        }
        if (func_001CF0C8(chr, 0x61)) {
            g->blast = g->blastMax;
        }
        if (BtlChar_TestMemberUnk70(chr)) {
            func_0024FFE8(obj);
        }
        break;
    case 0xF1:
    case 0xF2:
        /* fusion: merge the partner member into the current one */
        m = func_001CE050(chr);
        g = &m->gauge;
        other = func_001CE030(chr, chr->fusionMember);
        if (other != NULL) {
            og = &other->gauge;
            m->bonus[0] = BtlUtil_Clamp(m->bonus[0] + other->bonus[0], -20, 40);
            m->bonus[1] = BtlUtil_Clamp(m->bonus[1] + other->bonus[1], -20, 40);
            m->bonus[2] = BtlUtil_Clamp(m->bonus[2] + other->bonus[2], -20, 40);
            m->bonus[3] = BtlUtil_Clamp(m->bonus[3] + other->bonus[3], -20, 40);
            m->bonus[4] = BtlUtil_Clamp(m->bonus[4] + other->bonus[4], -20, 40);
            m->bonus[5] = BtlUtil_Clamp(m->bonus[5] + other->bonus[5], -20, 40);
            m->bonus[6] = BtlUtil_Clamp(m->bonus[6] + other->bonus[6], -20, 40);
            for (i = 0; i < 4; i++) {
                m->ability[i] |= other->ability[i];
            }
            g->health += og->health;
            g->healthMax += og->healthMax;
            g->blastMax = gBtlChars->tbl[4][m->chara * 4 + 3] * 100000;
            if (func_0020E280(chr) & 1) {
                g->health += 5000;
            }
            if (func_0020E280(chr) & 2) {
                g->health += 10000;
            }
            if (func_0020E280(chr) & 4) {
                g->ki = g->kiMax;
            }
            if (func_0020E280(chr) & 8) {
                BtlChar_SetHeldFlag(chr, 6);
                g->unk1C = 30000;
                g->ki = g->kiMax;
            }
            g->health = BtlUtil_Clamp(g->health, 1, g->healthMax);
            g->blast = BtlUtil_Clamp(g->blast, 0, g->blastMax);
            other->present = 0;
            og->health = 0;
        }
        g->unk34 = 1;
        m->chara = chr->newChara;
        m->costume = chr->newCostume;
        g->unk20 = chr->new20;
        break;
    case 0x104:
    case 0x106:
    case 0x107:
    case 0x108:
    case 0x12D:
    case 0x12E:
    case 0x12F:
    case 0x139:
    case 0x13A:
    case 0x13B:
        m = func_001CE050(chr);
        m->chara = chr->newChara;
        g = &m->gauge;
        m->costume = chr->newCostume;
        g->unk20 = chr->new20;
        if (BtlChar_TestFlag(chr, 0xA6)) {
            g->unk30 = 1;
            func_0024FFE8(obj);
            func_001CDD40(chr, func_001CE468(chr), 0, 0, 0.0f);
            g->health = g->healthMax;
        }
        break;
    }
    func_0024FE78(obj, func_0020E480(chr), -1);
    if (func_001CE1B8(chr)->unk20 != 0) {
        obj->flags |= 0x40000000;
    } else {
        obj->flags &= ~0x40000000;
    }
}

/* Clears the pose block and the per-stage state after a stage change. */
void BtlChar_OnStageLoaded(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    memset(&chr->pose, 0, 0x2D0);
    func_001D7570(chr);
    BtlChar_ClearFlag(chr, 0xE);
    BtlChar_SetHeldFlag(chr, 0xF);
    chr->unk15D4[0] = 0;
    chr->unk15D4[1] = 0;
    chr->unk15D4[2] = 0;
    chr->unk15D4[3] = 0;
    chr->unk15D4[4] = 0;
    func_001D6438(chr);
    func_001D7198(chr, 1);
    if ((func_001C4650(func_001C4638(chr)) & 0x10) || chr->unkFB0 >= 3) {
        BtlChar_SetHeldFlag(chr, 0x13A);
    } else {
        BtlChar_SetHeldFlag(chr, 0x139);
    }
    chr->unkFB0 = 1;
    func_001C3E60(chr, 0, 0.0f);
    func_0024C958(obj);
    func_0024E3F8(obj);
}

/* Puts the fighter back to the round start action (0xF9) with its timers cleared. */
void BtlChar_ResetRound(BtlMgrChr *chr) {
    s32 prev;

    if (chr->unk1330 != 0) {
        func_001D3568(chr);
    }
    BtlChar_ClearFlag(chr, 0xE);
    BtlChar_SetHeldFlag(chr, 0xF);
    BtlChar_SetHeldFlag(chr, 5);
    BtlChar_ClearFlag(chr, 0x125);
    func_001D7570(chr);
    chr->prevAction = chr->action;
    chr->action = 0xF9;
    chr->unk94C = -1;
    func_001E0310(chr);
    chr->unkFB0 = 1;
    chr->unkD58 = 0.5f;
    chr->unkD5C = 500.0f;
    chr->freezeNext = 0;
    chr->freezeDelay = 0;
    chr->freeze = 0;
    func_001D6438(chr);
    if (Battle_GetMode() == 3 && chr->side == 0) {
        func_001CEB40(chr, 10000);
    }
}

/* Resets the work area of the fighter's object. */
void BtlChar_ResetObjWork(BtlMgrChr *chr) {
    func_00250CD0(BtlChar_GetObj(chr));
}

/* Decides which fighters are frozen (hit-stop) and counts the freeze timers down. */
void BtlChars_UpdateFreeze(void) {
    s32 level[2];
    s32 i;
    s32 max;
    u8 *bytes;
    BtlMgrChr *chr;

    max = 0;
    bytes = (u8 *)level;
    memset(bytes, 0, sizeof(level));
    for (i = 0; i < BtlChar_GetCount(); i++) {
        chr = BtlChar_Get(i);
        if (BtlChar_TestFlag(chr, 0x125)) {
            level[i] = 2;
        } else if (BtlChar_TestFlag(chr, 0x126)) {
            level[i] = 1;
        }
        if (max < level[i]) {
            max = level[i];
        }
    }
    if (max > 0) {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            chr = BtlChar_Get(i);
            chr->freezeNext = 1;
            chr->freezeDelay = 0;
        }
        for (i = 0; i < BtlChar_GetCount(); i++) {
            chr = BtlChar_Get(i);
            if (max == level[i]) {
                chr->freezeNext = 0;
                chr->freezeDelay = 0;
                if (max >= 2) {
                    break;
                }
            }
            if (BtlChar_TestFlag(chr, 0x127)) {
                if (max == 1) {
                    chr->freezeNext = 0;
                    chr->freezeDelay = 0;
                }
            }
        }
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        chr = BtlChar_Get(i);
        if (chr->freeze > 0) {
            s32 v = chr->freeze - 1;

            if (v < 0) {
                v = 0;
            }
            chr->freeze = v;
        }
        if (chr->freezeNext != 0) {
            if (chr->freezeDelay != 0) {
                chr->freezeDelay--;
            } else {
                chr->freeze = chr->freezeNext;
                chr->freezeNext = 0;
            }
        }
    }
}

/* Sets bit 1 of every fighter object's flags: for the fighter with flag 0x12A only, or for all without one. */
void BtlChars_UpdateObjFlag2(void) {
    s32 i;
    s32 found = -1;
    BtlMgrChr *chr;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        if (BtlChar_TestFlag(BtlChar_Get(i), 0x12A)) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            chr = BtlChar_Get(i);
            if (BtlChar_TestFlag(chr, 0x30) || BtlChar_TestFlag(chr, 0xB)) {
                BtlChar_GetObj(chr)->flags &= ~2;
            } else {
                BtlChar_GetObj(chr)->flags |= 2;
            }
        }
    } else {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            chr = BtlChar_Get(i);
            if (i == found) {
                if (BtlChar_TestFlag(chr, 0x30) || BtlChar_TestFlag(chr, 0xB)) {
                    BtlChar_GetObj(chr)->flags &= ~2;
                } else {
                    BtlChar_GetObj(chr)->flags |= 2;
                }
            } else {
                BtlChar_GetObj(chr)->flags &= ~2;
            }
        }
    }
}

/* Clears bit 1 of every fighter object's flags. */
void BtlChars_ClearObjFlag2(void) {
    s32 i;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_GetObj(BtlChar_Get(i))->flags &= ~2;
    }
}

/* Runs the fighter-against-fighter pass, or the per-fighter fallback while somebody is frozen. */
void BtlChars_UpdateCollision(void) {
    s32 i;
    s32 nobodyFrozen = 1;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        if (BtlChar_IsFrozen(BtlChar_Get(i))) {
            nobodyFrozen = 0;
            break;
        }
    }
    if (nobodyFrozen) {
        func_001AF740();
        func_001CA520();
    } else {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            func_001CA618(BtlChar_Get(i));
        }
    }
}

/* Raises this frame's battle events for one fighter. */
void BtlChar_RaiseEvents(BtlMgrChr *chr) {
    BtlMgrGauge *g = func_001CE1B8(chr);
    s32 lost = g->healthMax - g->health;
    s32 tech;

    if (lost >= 70000) {
        BtlEvent_Raise(chr->side, 0x1C);
    } else if (lost >= 60000) {
        BtlEvent_Raise(chr->side, 0x1B);
    } else if (lost >= 50000) {
        BtlEvent_Raise(chr->side, 0x1A);
    } else if (lost >= 40000) {
        BtlEvent_Raise(chr->side, 0x19);
    } else if (lost >= 30000) {
        BtlEvent_Raise(chr->side, 0x18);
    } else if (lost >= 20000) {
        BtlEvent_Raise(chr->side, 0x17);
    } else if (lost >= 10000) {
        BtlEvent_Raise(chr->side, 0x16);
    }
    if (g->ki == g->kiMax) {
        BtlEvent_Raise(chr->side, 0x1D);
    }
    if (g->blast == g->blastMax) {
        BtlEvent_Raise(chr->side, 0x1E);
    }
    if (func_001DACE8(chr, 6)) {
        BtlEvent_Raise(chr->side, 0x1F);
    }
    if (func_00206B70(chr->objId)) {
        switch (func_001E0430(chr)) {
        case 2:
            BtlEvent_Raise(chr->side, 0x26);
            break;
        case 3:
            BtlEvent_Raise(chr->side, 0x27);
            break;
        case 4:
            BtlEvent_Raise(chr->side, 0x28);
            break;
        }
    }
    if (func_00206CC0(chr->objId)) {
        switch (func_001E0430(chr)) {
        case 0:
            BtlEvent_Raise(chr->side, 0x24);
            break;
        case 1:
            BtlEvent_Raise(chr->side, 0x25);
            break;
        }
    }
    if (BtlChar_TestFlag(chr, 8)) {
        switch (func_001E0358(chr)) {
        case 0x130:
        case 0x131:
        case 0x132:
            BtlEvent_Raise(chr->side, 0x14);
            break;
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
            BtlEvent_Raise(chr->side, 0x42);
            break;
        case 0x41:
            BtlEvent_Raise(chr->side, 0x44);
            break;
        case 0x42:
            BtlEvent_Raise(chr->side, 0x45);
            break;
        case 0x19:
            if (chr->unkD68 > 0) {
                BtlEvent_Raise(chr->side, 0x31);
            }
            break;
        case 0xFA:
        case 0xFC:
            BtlEvent_Raise(chr->side, 0x15);
            break;
        case 0x43:
            BtlEvent_Raise(chr->side, 0x21);
            break;
        }
    }
    if (func_001DACE8(chr, 0x5B)) {
        tech = func_0020C9F0(chr);
        switch (tech) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
            BtlEvent_Raise(chr->side, 0x3D);
            break;
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
            BtlEvent_Raise(chr->side, 0x34);
            break;
        case 0x3E:
        case 0x3F:
        case 0x40:
        case 0x41:
        case 0x42:
            BtlEvent_Raise(chr->side, 0x3E);
            func_001DAFB0(chr, 0x20);
            if (chr->unkD70 > 0) {
                func_001DAFB0(chr, 0x100000);
            }
            break;
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
            BtlEvent_Raise(chr->side, 0x32);
            func_001DAFB0(chr, 0x20);
            if (chr->unkD68 > 0) {
                func_001DAFB0(chr, 0x100000);
            }
            break;
        case 0x55:
            BtlEvent_Raise(chr->side, 0x33);
            break;
        case 0x23:
        case 0x25:
        case 0x27:
            BtlEvent_Raise(chr->side, 0x40);
            break;
        case 0x29:
        case 0x2B:
        case 0x2D:
            BtlEvent_Raise(chr->side, 0x56);
            break;
        case 0x8A:
            BtlEvent_Raise(chr->side, 0x3F);
            break;
        case 0x53:
            BtlEvent_Raise(chr->side, 0x4B);
            func_001DAFB0(chr, 0x80000);
            break;
        case 0x10:
        case 0x52:
        case 0x58:
        case 0x60:
        case 0x65:
        case 0x6F:
        case 0x72:
        case 0x74:
        case 0x7B:
        case 0x7D:
        case 0x90:
        case 0x92:
            BtlEvent_Raise(chr->side, 0x4B);
            break;
        case 0x75:
        case 0x76:
        case 0x77:
        case 0x78:
            func_001DAFB0(chr, 0x2000000);
            break;
        }
        switch (tech) {
        case 0x1D:
        case 0x3E:
        case 0x67:
            BtlEvent_Raise(chr->side, 0x37);
            break;
        case 0x1E:
        case 0x3F:
        case 0x68:
            BtlEvent_Raise(chr->side, 0x38);
            break;
        case 0x1F:
        case 0x40:
        case 0x69:
            BtlEvent_Raise(chr->side, 0x39);
            break;
        case 0x20:
        case 0x41:
        case 0x6A:
            BtlEvent_Raise(chr->side, 0x3A);
            break;
        case 0x21:
        case 0x42:
        case 0x6B:
            BtlEvent_Raise(chr->side, 0x3B);
            break;
        }
    }
    if (BtlChar_TestFlag(chr, 0x70)) {
        BtlEvent_Raise(chr->side, 0x43);
    }
    if (func_001DACE8(chr, 0xBE)) {
        BtlEvent_Raise(chr->side, 0x23);
    }
    if (BtlChar_TestFlag(chr, 0xBF)) {
        BtlEvent_Raise(chr->side, 0x47);
    }
    if (BtlChar_TestFlag(chr, 0xC1)) {
        BtlEvent_Raise(chr->side, 0x46);
        func_001DAFB0(chr, 0x800000);
    }
    if (BtlChar_TestFlag(chr, 0x6C)) {
        BtlEvent_Raise(chr->side, 0x57);
    }
    if (BtlChar_TestFlag(chr, 3)) {
        BattleResult *res = BattleResult_GetPtr();
        s32 opp = func_001DB770(chr);

        if (res->unk1C[opp] < chr->unkD40) {
            res->unk1C[opp] = chr->unkD40;
        }
        if (res->unk24[opp] < chr->unkD44) {
            res->unk24[opp] = chr->unkD44;
        }
        res->health[chr->side] = func_001CEED8(chr) * 100.0f;
        if (func_001CE1B8(chr)->health == 1) {
            if (func_001CE3D8(BtlChar_Get(func_001DB770(chr))) <= 0) {
                BtlEvent_Raise(chr->side, 0x54);
            }
        }
    }
    if (BtlInput_IsPressed(chr, 0x2000) && BtlCtrl_CanAct(chr->side)) {
        BtlEvent_Raise(chr->side, 0x50);
    }
    if (BtlChar_TestFlag(chr, 7)) {
        BtlEvent_Raise(chr->side, 0x49);
    }
    if (func_001DACE8(chr, 0x84)) {
        func_001DAFB0(chr, 0x4000000);
    }
    if (func_001DACE8(chr, 0xA4)) {
        func_001DAFB0(chr, 0x1000000);
    }
}

/* When any fighter raised flag 0xF9, resets every fighter for the next round and the 0x2000 effect tasks. */
void BtlChars_CheckRoundReset(void) {
    s32 i;
    s32 any = 0;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        if (BtlChar_TestFlag(BtlChar_Get(i), 0xF9)) {
            any = 1;
            break;
        }
    }
    if (any) {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            BtlChar_ResetRound(BtlChar_Get(i));
        }
        BtlScene_Reset(BTL_SCENE_RESET_2000);
    }
}

/* Re-reads the bonus levels of each member whose request flag (0x103 + n) is set. */
void BtlChar_ApplyBonusRequests(BtlMgrChr *chr) {
    s32 i;

    for (i = 0; i < BTL_CHR_MEMBER_MAX; i++) {
        if (BtlChar_TestFlag(chr, i + 0x103)) {
            BtlChar_CopyMemberBonus(chr, i);
            BtlChar_ClearFlag(chr, i + 0x103);
        }
    }
}

/* Re-reads the ability words of each member whose request flag (0x108 + n) is set. */
void BtlChar_ApplyAbilityRequests(BtlMgrChr *chr) {
    s32 i;

    for (i = 0; i < BTL_CHR_MEMBER_MAX; i++) {
        if (BtlChar_TestFlag(chr, i + 0x108)) {
            BtlChar_CopyMemberAbility(chr, i);
            BtlChar_ClearFlag(chr, i + 0x108);
        }
    }
}

/* Phase 2 body: builds this frame's input record from the pad. */
void BtlChar_SampleInput(BtlMgrChr *chr) {
    if (!BtlChar_IsFrozen(chr)) {
        func_001DA970(chr, BTL_CHR_STAGE_SAMPLE);
        BtlInput_Sample(chr);
    }
}

/* Phase 3 body: keeps last frame's values, clears the per-frame state and consumes the input record. */
void BtlChar_BeginFrame(BtlMgrChr *chr) {
    if (BtlChar_IsFrozen(chr)) {
        BtlInput_Stub(chr);
        return;
    }
    func_001DA970(chr, BTL_CHR_STAGE_INPUT);
    chr->prevPose = chr->pose;
    func_001D70E8(chr);
    BtlChar_GetPos(chr)->unkA0 = 0;
    BtlChar_GetPos(chr)->unkA4 = 0;
    func_001D8080(chr);
    chr->prevAction = chr->action;
    chr->prev964 = chr->unk964;
    chr->prev974 = chr->unk974;
    func_001C4580(chr, 1.0f);
    func_001C45A8(chr, 1.0f);
    chr->unkD4C = 0;
    chr->unkD50 = 0;
    func_001D6578(chr, 0);
    BtlChar_TickVoiceTimers(chr);
    func_001CD9C8(chr);
    chr->prev1262 = chr->unk1262;
    memset(&chr->unk1262, 0, sizeof(chr->unk1262));
    if (BtlChar_TestFlag(chr, 6)) {
        chr->unkD6C = func_0020F188(chr);
        chr->unkD74 = func_0020F1B0(chr);
    } else {
        chr->unkD6C = 1;
        chr->unkD74 = 1;
    }
    if (func_001CF0C8(chr, 1)) {
        chr->unkD6C += 2;
    } else if (func_001CF0C8(chr, 0)) {
        chr->unkD6C += 1;
    }
    if (func_001CF0C8(chr, 3)) {
        chr->unkD74 += 2;
    } else if (func_001CF0C8(chr, 2)) {
        chr->unkD74 += 1;
    }
    chr->frameBits = 0;
    chr->unk1590 = -1;
    chr->unk1594 = -1;
    BtlInput_Update(chr);
    BtlChar_ApplyBonusRequests(chr);
    BtlChar_ApplyAbilityRequests(chr);
}

/* Phase 3 body: sets the held flag of the sequence state (1 intro, 2 ready, 3 fight, 4 after) and flag 0x135. */
void BtlChar_UpdateSeqFlags(BtlMgrChr *chr) {
    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    BtlChar_ClearFlag(chr, 1);
    BtlChar_ClearFlag(chr, 2);
    BtlChar_ClearFlag(chr, 3);
    BtlChar_ClearFlag(chr, 4);
    switch (BtlSeq_GetState()) {
    case 0:
    case 1:
        BtlChar_SetHeldFlag(chr, 1);
        break;
    case 2:
        BtlChar_SetHeldFlag(chr, 2);
        break;
    case 3:
        BtlChar_SetHeldFlag(chr, 3);
        break;
    case 4:
    case 5:
    case 6:
        BtlChar_SetHeldFlag(chr, 4);
        break;
    }
    func_001DAD98(chr);
    if (func_001D63A8()) {
        BtlChar_SetHeldFlag(chr, 0x135);
    } else {
        BtlChar_ClearFlag(chr, 0x135);
    }
    if (func_001DAD40(chr, 0x135)) {
        BtlChar_ResetObjWork(chr);
    }
    if (!BtlChar_TestFlag(chr, 0xAF)) {
        BtlChar_ClearFlag(chr, 0x80);
    }
}

/* Phase 4 body 1: seven func_001D75C8.. passes, then pose -> object, object update, object -> pose. */
void BtlChar_UpdateMotion(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    func_001DCB58(chr);
    if (BtlChar_TestFlag(chr, 0x2B)) {
        func_00250888(obj, 1);
    } else {
        func_00250888(obj, 0);
    }
    func_001D75C8(chr);
    func_001D7628(chr);
    func_001D7688(chr);
    func_001D7720(chr);
    func_001D7908(chr);
    func_001D79A0(chr);
    func_001D7A48(chr);
    func_001D7198(chr, 1);
    func_0024C958(obj);
    func_0024CC88(obj);
    func_001D73B0(chr);
    func_001D2D10(chr);
    func_0024E3F8(obj);
    func_001D70E8(chr);
    func_001D64A0(chr);
}

/* Phase 4 body 2 (stage 6). */
void BtlChar_UpdateStage6(BtlMgrChr *chr) {
    if (!BtlChar_IsFrozen(chr)) {
        func_001DA970(chr, BTL_CHR_STAGE_6);
        func_001DBCE0(chr);
        func_001D35B0(chr);
    }
}

/* Phase 4 body 3 (stage 7). */
void BtlChar_UpdateStage7(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    if (!BtlChar_IsFrozen(chr)) {
        func_001DA970(chr, BTL_CHR_STAGE_7);
        func_001DF4A8(chr);
        func_001DF210(chr);
        func_001D7198(chr, 0);
        func_0024E3F8(obj);
        func_001D70E8(chr);
    }
}

/* Phase 4 body 4 (stage 8). */
void BtlChar_UpdateStage8(BtlMgrChr *chr) {
    BtlMgrObj *obj;
    s32 flag = 1;
    s32 started;

    obj = BtlChar_GetObj(chr);
    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    func_001DA970(chr, BTL_CHR_STAGE_8);
    func_001DEF70(chr);
    func_001D7198(chr, 0);
    func_0024E3F8(obj);
    if (BtlChar_TestFlag(chr, 0x55)) {
        func_0024DD80(obj, chr->unk1310);
        flag = 0;
    }
    started = 0;
    if (gBtlChars->flags & BTL_CHARS_STARTED) {
        started = 1;
    }
    if (func_001D63A8()) {
        started = 0;
    }
    func_001B21A8(obj, flag, func_00206E88(chr->objId) ? 0 : started);
    func_001B15B8(obj);
    func_001D70E8(chr);
}

/* Phase 4 body 5 (stage 9). */
void BtlChar_UpdateStage9(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    if (!BtlChar_IsFrozen(chr)) {
        func_001DA970(chr, BTL_CHR_STAGE_9);
        func_001CD558(chr);
        func_001D7198(chr, 0);
        func_0024E3F8(obj);
    }
}

/* Phase 4 body 6: the fighter's own camera, then the object's final placement. */
void BtlChar_UpdateCamera(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    if (!BtlChar_IsFrozen(chr)) {
        ChrCam_StartCut(chr);
        ChrCam_UpdateDemo(chr);
        ChrCam_UpdateInput(chr);
        func_00250CB8(obj, BtlChar_GetPos(chr)->unk30);
        func_00250D38(obj);
        func_001D7088(chr);
        func_0024E3F8(obj);
        func_001D2D30(chr);
        func_0024DDF0(obj);
    }
}

/* Phase 4 body 7 (stage 10). */
void BtlChar_UpdateStage10(BtlMgrChr *chr) {
    if (!BtlChar_IsFrozen(chr)) {
        func_001DA970(chr, BTL_CHR_STAGE_10);
        func_001D3668(chr);
        func_001D2E88(chr);
    }
}

/* Phase 5 body: after the effect scene: low-health state and the vector pushed onto the opponent. */
void BtlChar_PostScene(BtlMgrChr *chr) {
    BtlMgrChr *opp;

    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    func_001D70E8(chr);
    ChrCam_Update(chr);
    func_001D2EC8(chr);
    if (func_001CE1B8(chr)->health < 10000) {
        func_001CE1B8(chr)->lowHealth = 1;
        if (!(func_0020E108(chr) & 0x80) && chr->unkE14 <= 0 && chr->unkE18 <= 0 && !BtlChar_TestFlag(chr, 6)) {
            func_001CE1B8(chr)->lowHealthIdle = 1;
        } else {
            func_001CE1B8(chr)->lowHealthIdle = 0;
        }
    } else {
        func_001CE1B8(chr)->lowHealthIdle = 0;
        func_001CE1B8(chr)->lowHealth = 0;
    }
    opp = BtlChar_Get(func_001DB770(chr));
    if (BtlChar_TestFlag(chr, 0x97)) {
        if (!BtlChar_TestFlag(chr, 0x30)) {
            func_001D8270(chr, opp->unkF30, 1, 4);
        } else {
            func_00121E20(opp->unkF30);
        }
    } else {
        func_00121E20(opp->unkF30);
    }
}

/* Phase 6 body 1: late per-fighter updates and the status-table penalty while gauge.unk30 is on. */
void BtlChar_UpdateLate(BtlMgrChr *chr) {
    BtlMgrObj *obj = BtlChar_GetObj(chr);

    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    func_001D2EF0(chr);
    BtlChar_StopVoiceOnFlag(chr);
    func_001DA5A8(chr);
    func_001DC9A0(chr);
    func_001C31A0(chr);
    if (BtlChar_TestMemberUnk70(chr)) {
        OBJ_WORD_18028(obj) = 1;
        func_001C33D8(chr, 0, -10, 1);
        func_001C33D8(chr, 1, -10, 1);
        func_001C33D8(chr, 2, -10, 1);
        func_001C33D8(chr, 3, -10, 1);
    } else {
        OBJ_WORD_18028(obj) = 0;
    }
}

/* Phase 6 body 2 (stages 11, 12): final pose vectors and the frame's battle events. */
void BtlChar_EndFrame(BtlMgrChr *chr) {
    BtlMgrPose *pose = BtlChar_GetPos(chr);

    if (BtlChar_IsFrozen(chr)) {
        return;
    }
    func_001DA970(chr, BTL_CHR_STAGE_EVENTS);
    if (!BtlChar_TestFlag(chr, 0x24)) {
        func_001D8270(chr, pose->unk30, 0, 1);
        func_001D82E0(chr, pose->unk40, 0);
    } else {
        func_00121E20(pose->unk30);
        func_00121E20(pose->unk40);
    }
    BtlChar_RaiseEvents(chr);
    BtlChar_UpdateVibration(chr);
    func_001DA970(chr, BTL_CHR_STAGE_END);
}

/* Allocates the roster for `count` fighters and resets it. */
void BtlChar_AllocAll(s32 count) {
    gBtlChars = Heap_Alloc(sizeof(BtlCharMgr), 0x20, 0, HEAP_ANY);
    memset(gBtlChars, 0, sizeof(BtlCharMgr));
    gBtlChars->chars = Heap_Alloc(count * 0x1600, 0x20, 0, HEAP_ANY);
    memset(gBtlChars->chars, 0, count * 0x1600);
    gBtlChars->unk8 = Heap_Alloc(count * 0x34, 0x20, 0, HEAP_ANY);
    memset(gBtlChars->unk8, 0, count * 0x34);
    gBtlChars->unkC = Heap_Alloc(count * 0x34, 0x20, 0, HEAP_ANY);
    memset(gBtlChars->unkC, 0, count * 0x34);
    gBtlChars->count = count;
    gBtlChars->tbl[0] = FILE_TABLE(gCommonRes->data[0], 3);
    gBtlChars->tbl[1] = FILE_TABLE(gCommonRes->data[0], 5);
    gBtlChars->tbl[2] = FILE_TABLE(gCommonRes->data[0], 6);
    gBtlChars->tbl[3] = FILE_TABLE(gCommonRes->data[0], 7);
    gBtlChars->tbl[4] = FILE_TABLE(gCommonRes->data[0], 8);
    gBtlChars->tbl[5] = FILE_TABLE(gCommonRes->data[0], 9);
    BtlChar_ResetAll();
    func_001D9F90();
}

/* Frees the roster. */
void BtlChar_FreeAll(void) {
    Heap_Free(gBtlChars->unkC);
    Heap_Free(gBtlChars->unk8);
    Heap_Free(gBtlChars->chars);
    Heap_Free(gBtlChars);
    gBtlChars = NULL;
}

/* Zeroes the manager's state and every fighter, then sets each fighter up from the battle setup. */
void BtlChar_ResetAll(void) {
    s32 i;
    BtlMgrChr *chr;
    s32 pad;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        func_001D3568(BtlChar_Get(i));
    }
    gBtlChars->frame = 0;
    gBtlChars->unk18 = 0;
    gBtlChars->flags = 0;
    gBtlChars->unk1C = 0;
    memset(gBtlChars->unkA0, 0, 0x90);
    memset(gBtlChars->unk138, 0, 0x140);
    memset(gBtlChars->unk40, 0, 0x60);
    gBtlChars->unk130 = 0;
    memset(gBtlChars->chars, 0, BtlChar_GetCount() * 0x1600);
    func_001D6150();
    BtlReplay_ResetViewer();
    for (i = 0; i < BtlChar_GetCount(); i++) {
        chr = BtlChar_Get(i);
        chr->side = i;
        pad = BattleSide_GetPad(i);
        chr->index = i;
        chr->pad = pad;
        BtlChar_BindObject(chr);
        BtlChar_Reset(chr);
    }
}

/* Loader callback: a side's new character model is ready. */
void BtlChars_OnModelLoaded(s32 side) {
    BtlMgrChr *chr = BtlChar_Get(side);

    if (chr != NULL) {
        BtlChar_OnModelLoaded(chr);
    }
}

/* Loader callback: a new stage is ready. */
void BtlChars_OnStageLoaded(void) {
    s32 i;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_OnStageLoaded(BtlChar_Get(i));
    }
}

/* Phase 2: samples the pads into every fighter's input record. */
void BtlChars_SampleInput(void) {
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_SampleInput(BtlChar_Get(i));
    }
}

/* Phase 1: resets the roster the first frame the sequence reaches Ready or Fight; returns 1 when it did. */
s32 BtlChars_CheckStart(void) {
    s32 ret = 0;

    if (BtlSeq_GetState() == 2 || BtlSeq_GetState() == 3) {
        if (!(gBtlChars->flags & BTL_CHARS_STARTED)) {
            if (Battle_GetMode() != 1) {
                BtlChar_ResetAll();
                ret = 1;
            }
        }
        gBtlChars->flags |= BTL_CHARS_STARTED;
    }
    return ret;
}

/* Phase 3: frame counter, round reset, hit-stop, then input and per-frame clears for every fighter. */
void BtlChars_UpdateInput(void) {
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        BtlChars_ClearObjFlag2();
        return;
    }
    if ((gBtlChars->flags & BTL_CHARS_STARTED) && !func_001D63A8()) {
        gBtlChars->frame++;
        gBtlChars->frame &= 0x3FFFFFFF;
        gBtlChars->unk1C = BtlUtil_Max(gBtlChars->unk1C - 1, 0);
    }
    BtlChars_CheckRoundReset();
    BtlChars_UpdateFreeze();
    func_001DA058();
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_BeginFrame(BtlChar_Get(i));
    }
    func_001D8128(0);
    func_001DAE98();
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateSeqFlags(BtlChar_Get(i));
    }
}

/* Phase 4: the simulation proper, as seven passes over the roster. */
void BtlChars_UpdateMain(void) {
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateMotion(BtlChar_Get(i));
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateStage6(BtlChar_Get(i));
    }
    func_001D7C30();
    func_001D8128(1);
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateStage7(BtlChar_Get(i));
    }
    func_001D8128(2);
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateStage8(BtlChar_Get(i));
    }
    func_001D8128(3);
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateStage9(BtlChar_Get(i));
    }
    func_001D8128(4);
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateCamera(BtlChar_Get(i));
    }
    BtlReplay_UpdateViewer();
    func_001D3110();
    func_001CF220();
    BtlChars_UpdateCollision();
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateStage10(BtlChar_Get(i));
    }
    func_001D9900();
}

/* Phase 5: per-fighter pass after the effect scene. */
void BtlChars_PostScene(void) {
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_PostScene(BtlChar_Get(i));
    }
}

/* Phase 6: late updates, object flags, queued requests and the frame's battle events. */
void BtlChars_EndFrame(void) {
    s32 i;

    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return;
    }
    func_001D6178();
    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_UpdateLate(BtlChar_Get(i));
    }
    BtlChars_UpdateObjFlag2();
    func_001DA098();
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlChar_EndFrame(BtlChar_Get(i));
    }
    func_001D8128(5);
}
