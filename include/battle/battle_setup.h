#ifndef BATTLE_BATTLE_SETUP_H
#define BATTLE_BATTLE_SETUP_H

#include "types.h"
#include "battle/btl_seq.h"

/*
 * Battle result finish, battle events, battle setup and replay block:
 * src/battle/battle_setup.c = 0x129170..0x12B570 (second half of the file that starts at 0x126EC8).
 *
 * The structs here are this file's own view of the battle work (battle/battle.h's BattleWork cannot be
 * edited from here); the field offsets are what the matching C reads and writes. "verified" below means
 * exactly that; meanings come from the callers and are marked as inference where they are.
 *
 * --- How a battle is set up -------------------------------------------------------------------------
 * The menu overlay (and two places in the main executable) build the setup with this sequence:
 *     Battle_ClearWork()                       -> BattleSetup_Clear(): memset + magic "btls" + version 7
 *     BattleSetup_SetRule(screenMode, mode, bgm, timeLimit, announcer, stage, unk10)
 *     BattleSetup_SetSide(side, control, pad, memberCount, unk1FC, unk200, lead, charaBits)   for side 0, 1
 *     BattleSetup_SetMember(side, idx, chara, costume, variant, cpuLevel, health, items)       per member
 *     [BattleSetup_SetPoolMember(count, idx, ...)   mode 3 only: the queue of up to 50 opponents]
 *     BattleSetup_Finish()                     -> BattleSetup_FinishEx(0)
 * Then Game_Main leaves the overlay and runs Battle_Main, which reads the setup through the getters.
 * Callers: menu 0x348710 and 0x351508 (mode 0 / 6), 0x35CFD0 (mode 5), 0x36A720 (mode 4), 0x372560 and
 * 0x388EE0 (mode 3), 0x373A68, 0x37A060 and 0x37F978 (mode 2); main 0x2617F0 (mode 7, the attract demo);
 * the battle script commands 0x25B900 / 0x25B9A0 / 0x25BA08 (mode 1), whose BattleSetup_Finish() is
 * called by the loader (BtlLoad_StepInitial) once the script has run.
 *
 * --- Battle mode (BtlRule.mode), as far as the callers show ------------------------------------------
 *   0  versus battle from the menu ("duel"): the only mode with rule options from the save (time limit
 *      rule[0], CPU strength rule[1], announcer rule[2]) and, with mode 4, the only one that may be
 *      split-screen. Equal health at time up is a draw only here (BtlSeq_CanDraw).
 *   1  scripted battle: set up by the commands of the battle script file 0x1FF + n (BattleSetup_SetScript).
 *      Side 0 is the pad, side 1 the CPU; every character is usable; CPU level is raised by the
 *      difficulty (gProgress->0x3C: +6 / +9); BattleSetup_FinishEx stores the first missing bit of
 *      SaveData.unlockFlags (1..7) in rule.unk18. Sequence table gBtlSeqTblMode1.
 *   2  three menu screens (0x373A68, 0x37A060, 0x37F978): pad against CPU team(s). Which game mode each
 *      is was not established.
 *   3  two menu screens (0x372560, 0x388EE0): one pad member against the opponent pool (BtlMemberPool,
 *      up to 50 members, bonus levels up to +60); pool entry 0 becomes side 1's member 0.
 *   4  menu 0x36A720: one member per side, split-screen allowed, rule.unk10 = 1.
 *   5  menu 0x35CFD0: no time limit, announcer 7, both sides one member; short sequence table.
 *   6  the versus menu when gProgress->0x18 == 0x2D: no time limit, one member per side, side 0 pad,
 *      side 1 CPU with cpuLevel -1 (does nothing), restart on result reason bit 2: training (inference).
 *   7  attract demo (main 0x2617F0): 45 s, both sides CPU, 9 fixed character pairs; the fight state goes
 *      straight to the end state.
 *   8  never passed to BattleSetup_SetRule by a caller found; read by the sequence (no winner scene) and
 *      by 0x23EE08, which looks at which sides are pads.
 *   9  only seen in BattleSetup_FixForMode: both sides CPU at level 29 with AI type 0, no time limit, then
 *      the mode becomes 6. Never passed to BattleSetup_SetRule by a caller found.
 *   Modes 8 and 9 can only arrive through a setup written some other way (a loaded replay block carries
 *   its own setup, BattleReplay_Load).
 *
 * --- Events ------------------------------------------------------------------------------------------
 * Each side has a set of up to 128 event bits (BtlEventSet): `now` collects BtlEvent_Raise() calls,
 * BtlEvent_Update() (once per unpaused frame, from Battle_UpdateWork) moves it to `prev` and ORs it into
 * `held`. BtlEvent_IsNew() is a rising edge (now & ~prev); BtlEvent_WasRaised() reads `held`, which is
 * what the result's event summary is built from (BattleResult_CollectEvents).
 * Events 0..18 are time events (BtlEvent_RaiseTimeEvents): 10, 15, ... 180 seconds on the sub clock.
 * Other ids seen here: 0x48 (BattleResult_CountFrame), 0x4C (raised on both sides when a requested
 * interrupt can start), 0x4D / 0x4E (clear the wait flag), 0x4F (calls 0x12BDA8 -> 0x23DE40),
 * 0x5A / 0x5B (forwarded to the script module, 0x259910).
 *
 * --- Replay ------------------------------------------------------------------------------------------
 * gBattleReplay (0x1ABA8 bytes at 0x301268) = a copy of the setup + recorded data + three words.
 * BattleSetup_FinishEx copies the finished setup into it; the memory card code saves it
 * (BattleReplay_GetBuffer, caller 0x11E608) and loads it back (BattleReplay_Load, caller 0x11DA38), which
 * also makes its setup the current one. `active` is tested by the input, camera and HUD code;
 * BattleResult_Set stores 0 / 1 in it for result reason bits 15 / 16.
 */

enum {
    BTL_MODE_VERSUS = 0,
    BTL_MODE_SCRIPT = 1,
    BTL_MODE_2 = 2,
    BTL_MODE_POOL = 3,
    BTL_MODE_4 = 4,
    BTL_MODE_5 = 5,
    BTL_MODE_TRAINING = 6, /* guess */
    BTL_MODE_DEMO = 7,
    BTL_MODE_8 = 8,
    BTL_MODE_9 = 9
};

/* BtlSide.control */
#define BTL_CONTROL_PAD 0
#define BTL_CONTROL_CPU 2 /* 1 was not seen */

#define BTL_SETUP_MAGIC 0x736C7462 /* "btls" */
#define BTL_SETUP_VERSION 7
#define BTL_MEMBER_MAX 5
#define BTL_POOL_MAX 50
#define BTL_BGM_RANDOM 24      /* BattleSetup_SetRule: pick rand() % 24 */
#define BTL_CPU_LEVEL_MAX 29   /* func_00261738 maps the 5 difficulty settings to 0, 6, 13, 21, 29 */
#define BTL_TIME_EVENT_COUNT 19

/* Equipped items of a member: 1-based item ids, 0 = empty (same layout as SaveCustom.item[set]). */
typedef struct BtlItemSet {
    /* 0x00 */ u16 id[8];
} BtlItemSet; /* size 0x10 */

/* One team member. All offsets verified. */
typedef struct BtlMember {
    /* 0x00 */ s32 chara;       /* character id (file 8 + chara * 2 + side, model 0x590 + chara * 10 + costume) */
    /* 0x04 */ s32 costume;
    /* 0x08 */ s32 variant;     /* non-zero: model file + 4 (battle_work.c); passed to func_001CDD40 at fighter init */
    /* 0x0C */ s32 cpuLevel;    /* 0..29, -1 for the training opponent; copied to the fighter's member entry + 0x38 */
    /* 0x10 */ f32 health;      /* 100.0f from every menu caller, an integer script argument: percent (inferred) */
    /* 0x14 */ BtlItemSet items;
    /* 0x24 */ s32 bonus[8];    /* levels clamped to -20..20 (or 60): [0] = 0, [1] = [4] = item sum 2, [2] = sum 0,
                                   [3] = sum 1, [5] = [6] = [7] = sum 3 (the four s16 at ItemInfo + 0xC) */
    /* 0x44 */ s32 aiType;      /* item id - 0x87 of an equipped item whose ItemInfo byte 0 is 2, else the character
                                   table's first word; copied to the fighter's member entry + 0x3C */
    /* 0x48 */ s32 ability[4];  /* OR of the four words at ItemInfo + 0x18 of every equipped item */
    /* 0x58 */ void *data;      /* loader: the 0x1000-byte block in use (battle_work.h) */
    /* 0x5C */ void *buf[2];    /* loader: two 0x1000-byte heap blocks */
} BtlMember; /* size 0x64 */

/* Character, costume and model variant of a side's fighter. */
typedef struct BtlForm {
    /* 0x00 */ s32 chara;
    /* 0x04 */ s32 costume;
    /* 0x08 */ s32 variant;
} BtlForm; /* size 0xC */

/* Characters a side may turn into (bit = character id). Only words 0..2 are ever written. */
typedef struct BtlCharaBits {
    /* 0x00 */ u64 bits[8];
} BtlCharaBits; /* size 0x40 */

/* One side of the battle. All offsets verified. */
typedef struct BtlSide {
    /* 0x000 */ s32 memberCount; /* team size, 1..5 (fighter + 0x998) */
    /* 0x004 */ BtlMember members[BTL_MEMBER_MAX];
    /* 0x1F8 */ s32 lead;        /* index of the member that starts the battle */
    /* 0x1FC */ s32 unk1FC;      /* fighter + 0x1300. 1, or the inverted save rule[3] / rule[4] for a CPU side */
    /* 0x200 */ s32 unk200;      /* fighter + 0xCF4. 1 from most callers, 0 from menu 0x373A68 */
    /* 0x204 */ s32 pad;         /* controller number: fighter + 4, used as SAVE_FLAG_PAD_A(pad) */
    /* 0x208 */ s32 control;     /* BTL_CONTROL_* */
    /* 0x20C */ s32 unk20C;
    /* 0x210 */ BtlCharaBits charaBits; /* tested before a transformation / fusion (0x2033C8, 0x203788) */
    /* 0x250 */ BtlForm startForm; /* the lead member's, set by BattleSetup_FinishEx */
    /* 0x25C */ BtlForm form;      /* what is loaded now; the loader compares it with startForm at a restart */
    /* 0x268 */ s32 objId;         /* BtlObj_Get() argument of the side's fighter object */
    /* 0x26C */ s32 modelSlot;     /* result of func_00249C60(side, chara, costume, variant) */
} BtlSide; /* size 0x270 */

/* Battle rules: setup + 8. All offsets verified. */
typedef struct BtlRule {
    /* 0x00 */ s32 mode;       /* BTL_MODE_* */
    /* 0x04 */ s32 bgm;        /* BGM file 0x10B16 + bgm (Battle_ResetWork) */
    /* 0x08 */ s32 timeLimit;  /* index into gBattleTimeLimitTbl: 0 none, 1..5 = 60, 90, 180, 240, 45 s */
    /* 0x0C */ s32 announcer;  /* 0..7: announcement stream = base + announcer * 7 + n (0x22AB50) */
    /* 0x10 */ s32 unk10;      /* inverted save rule[5] in versus, 1 in mode 4; tested by 0x12D450 */
    /* 0x14 */ s32 stage;      /* stage the battle starts on, 0..34 */
    /* 0x18 */ s32 unk18;      /* mode 1: 1 + index of the first clear bit 0..6 of SaveData.unlockFlags, else 0 */
    /* 0x1C */ s32 screenMode; /* 1 = split-screen */
    /* 0x20 */ s32 curStage;   /* current stage (in-battle stage changes) */
} BtlRule; /* size 0x24 */

/* Options: setup + 0x2C. */
typedef struct BtlOption {
    /* 0x00 */ s32 isSet;      /* 0: BattleSetup_FinishEx fills optA / optB from the save data */
    /* 0x04 */ s32 optA[2];    /* per side, SaveData.unk1694 by default -> fighter + 0x49C */
    /* 0x0C */ s32 optB[2];    /* per side, SaveData.unk1698 by default -> fighter + 0x4B8 = (optB == 0) */
    /* 0x14 */ s32 unk14;      /* BattleSetup_SetOption14 / Battle_GetOption14, neither has a caller */
    /* 0x18 */ u8 unk18[0x78];
} BtlOption; /* size 0x90 */

/* The battle setup: the first 0x5A8 bytes of the battle work (Battle_GetSetup()). */
typedef struct BtlSetup {
    /* 0x000 */ u8 magic[4];   /* "btls" */
    /* 0x004 */ s32 version;   /* 7 */
    /* 0x008 */ BtlRule rule;
    /* 0x02C */ BtlOption option;
    /* 0x0BC */ s32 script;    /* battle script number + 1 (file 0x1FF + n), 0 = none */
    /* 0x0C0 */ BtlSide sides[2];
    /* 0x5A0 */ s32 unk5A0;    /* 1 after BattleSetup_FinishEx */
    /* 0x5A4 */ s32 unk5A4;
} BtlSetup; /* size 0x5A8 */

/* Opponent queue of mode 3: battle work + 0x5A8. */
typedef struct BtlMemberPool {
    /* 0x00 */ s32 cur;        /* reset to 0 */
    /* 0x04 */ s32 count;
    /* 0x08 */ BtlMember members[BTL_POOL_MAX];
} BtlMemberPool; /* size 0x1390 */

/* Result block: battle work + 0x1938. */
typedef struct BtlResult {
    /* 0x00 */ s32 flags;      /* BTL_RESULT_* (btl_seq.h) */
    /* 0x04 */ s32 reason;     /* BTL_REASON_*; bit 5 forces "side 1 won", bit 6 "side 0 won" at the end */
    /* 0x08 */ s32 unk8;       /* 1 when the battle was aborted */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ u64 eventSummary; /* built by BattleResult_CollectEvents */
    /* 0x18 */ s32 frames;     /* BattleResult_CountFrame */
    /* 0x1C */ s32 unk1C[2];   /* per side, zeroed at fighter init (0x1C02C8) */
    /* 0x24 */ s32 unk24[2];   /* same */
    /* 0x2C */ f32 health[2];  /* btl_seq.h */
    /* 0x34 */ BtlClock clock; /* copy of the battle clock at the end */
    /* 0x44 */ s32 unk44;
} BtlResult; /* size 0x48 */

typedef struct BtlEventSet {
    /* 0x00 */ u64 now[2];     /* raised since the last BtlEvent_Update */
    /* 0x10 */ u64 prev[2];    /* raised in the frame before */
    /* 0x20 */ u64 held[2];    /* raised at any time */
} BtlEventSet; /* size 0x30 */

/* Event work: battle work + 0x1980. */
typedef struct BtlEvents {
    /* 0x00 */ s32 script;     /* script handle (battle_work.h), kept by BtlEvent_Reset */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ BtlEventSet set[2];
    /* 0x68 */ s32 interrupt;  /* 1: waiting for BtlFacade_AreBothInterruptible() to raise event 0x4C */
    /* 0x6C */ s32 waitFlag;   /* 1 until event 0x4D or 0x4E is new on a side */
} BtlEvents; /* size 0x70 */

typedef struct BtlReplayData {
    /* 0x00000 */ u64 unk0[0x1A5F0 / 8]; /* 8-byte aligned: the block copy uses ld/sd without an alignment test */
} BtlReplayData; /* size 0x1A5F0 */

/* What BattleReplay_GetData() points at: the data and the flag word behind it. */
typedef struct BtlReplayRec {
    /* 0x00000 */ BtlReplayData data;
    /* 0x1A5F0 */ s32 flags;   /* bit 0 */
} BtlReplayRec;

typedef struct BtlReplay {
    /* 0x00000 */ BtlSetup setup;
    /* 0x005A8 */ BtlReplayData data;
    /* 0x1AB98 */ s32 flags;
    /* 0x1AB9C */ s32 active;  /* D_0031BE04 */
    /* 0x1ABA0 */ s32 loaded;  /* D_0031BE08 */
    /* 0x1ABA4 */ s32 unk1ABA4;
} BtlReplay; /* size 0x1ABA8 */

extern BtlReplay gBattleReplay;
extern s16 gBattleTimeLimitTbl[8];

void BattleResult_CountFrame(void);
void BattleResult_Stub1291B8(void);
void BattleResult_Finish(void);
s32 BattleResult_GetFlags(void);
s32 BattleResult_GetReason(void);
u64 BattleResult_GetEventSummary(void);
BtlResult *BattleResult_GetPtr(void);
s32 BtlClock_ToSeconds(BtlClock *clock);

void BtlEvent_RaiseTimeEvents(void);
void BtlEvent_UpdateRequests(void);
void BtlEventSet_Rotate(BtlEventSet *set);
void BtlEvent_SetBit(u64 *bits, s32 n);
s32 BtlEventSet_IsNew(BtlEventSet *set, s32 n);
s32 BtlEventSet_WasRaised(BtlEventSet *set, s32 n);
void BtlEvent_ClearAll(void);
void BtlEvent_Reset(void);
void BtlEvent_Raise(s32 side, s32 ev);
s32 BtlEvent_IsNew(s32 side, s32 ev);
s32 BtlEvent_WasRaised(s32 side, s32 ev);
void BtlEvent_Update(void);
void BtlEvent_BeginInterrupt(void);
void BtlEvent_EndInterrupt(void);
void BtlEvent_SetWaitOff(s32 off);
s32 BtlEvent_IsWaitOff(void);

void BtlMember_ClampBonus(BtlMember *m, s32 wide);
void BtlMember_ApplyItems(BtlMember *m);
void BtlMember_Init(BtlMember *m, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health, BtlItemSet *items);

void BattleSetup_FixForMode(void);
void BattleSetup_InitCharaBits(BtlCharaBits *dst, BtlCharaBits *src);
void BattleSetup_DefaultOptions(void);
void BattleSetup_Clear(void);
void BattleReplay_ClearDataFlag(void);
void BattleSetup_SetScript(s32 script);
void BattleSetup_SetOptions(s32 optA0, s32 optA1, s32 optB0, s32 optB1);
void BattleSetup_SetOption14(s32 val);
void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                         BtlCharaBits *bits);
void BattleSetup_SetMemberByItemIds(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel,
                                    f32 health, u32 *itemIds);
void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                           BtlItemSet *items);
void BattleSetup_SetPoolMember(s32 count, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                               BtlItemSet *items);
void BattleSetup_Finish(void);
void BattleSetup_FinishEx(s32 wide);

BtlReplay *BattleReplay_GetBuffer(s32 *size);
s32 BattleReplay_Load(BtlReplay *buf, s32 size);
s32 BattleReplay_IsActive(void);
s32 BattleReplay_IsLoaded(void);
BtlReplayRec *BattleReplay_GetData(void);
s32 BattleReplay_TestDataFlag(void);
void BattleReplay_SetActive(s32 active);

s32 Battle_GetHumanSide(void);
s32 Battle_GetOption14(void);
s32 Battle_GetScript(void);
s32 Battle_IsSplitScreen(void);
s32 Battle_GetMode(void);
s32 Battle_GetTimeLimit(void);
s32 Battle_IsTimeLimitOff(void);
s32 Battle_GetAnnouncer(void);
s32 Battle_GetBgm(void);
s32 Battle_GetStartStage(void);
s32 Battle_GetStage(void);
void Battle_SetStage(s32 stage);
s32 Battle_IsStageChanged(void);
void Battle_ResetStage(void);
s32 Battle_GetRuleUnk10(void);

s32 BattleSide_GetOptionA(s32 side);
s32 BattleSide_GetOptionB(s32 side);
s32 BattleSide_GetPad(s32 side);
s32 BattleSide_GetControl(s32 side);
s32 BattleSide_IsCpu(s32 side);
s32 BattleSide_GetUnk200(s32 side);
s32 BattleSide_GetUnk1FC(s32 side);
s32 BattleSide_GetStartChara(s32 side);
s32 BattleSide_GetStartCostume(s32 side);
s32 BattleSide_GetStartVariant(s32 side);
s32 BattleSide_GetChara(s32 side);
void BattleSide_SetChara(s32 side, s32 chara);
s32 BattleSide_IsCharaChanged(s32 side);
void BattleSide_ResetChara(s32 side);
s32 BattleSide_GetMemberChara(s32 side, s32 idx);
s32 BattleSide_GetMemberCostume(s32 side, s32 idx);
s32 BattleSide_GetMemberVariant(s32 side, s32 idx);
s32 BattleSide_GetMemberCount(s32 side);
s32 BattleSide_GetObjId(s32 side);
s32 BattleSide_GetModelSlot(s32 side);
void BattleSide_SetObjId(s32 side, s32 objId);
void BattleSide_SetModelSlot(s32 side, s32 slot);
void BattleSide_SetForm(s32 side, s32 chara, s32 costume, s32 variant);
s32 BattleSide_IsFormChanged(s32 side);
BtlMember *BattleSide_GetMember(s32 side, s32 idx);
void BattleSide_SetMemberItems(s32 side, s32 idx, BtlItemSet *items);
s32 BattleSide_IsCharaUsable(s32 side, s32 chara);

#endif
