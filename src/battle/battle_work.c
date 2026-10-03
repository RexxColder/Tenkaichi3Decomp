#include "common.h"
#include "battle/battle_work.h"
#include "battle/battle_setup.h"
#include "sys/adx.h"
#include "sys/loading.h"

/* Battle work: the static BattleWork block and its accessors, reset, per-frame update, load / unload entry points.
 * 0x126EC8..0x127120. The loader itself is src/battle/battle_load.c (0x127120 onward).
 *
 * The two files are separate translation units: when Battle_GetWork (0x126EC8) or Battle_GetEventWork (0x127028)
 * is defined in the same file as the loader job step functions, ee-gcc 2.96 fills their branch delay slots
 * differently (e.g. `beq; nop; b end; li v0,1` instead of the original `beq; li v0,1; b end+4; ld s0`, and
 * `beqzl` instead of `beqz` in front of the Battle_GetEventWork call) and four of them cannot match. Where
 * exactly the boundary lies between 0x127048 and 0x1272B0 is not provable from the code (every function in
 * between matches on either side); 0x127120, the first function that is not a Battle_* entry point, was chosen. */

extern void *memset(void *dst, s32 c, u32 n);

extern BattleWork gBattleWork;
extern s32 D_002C6EC0[]; /* table handed to func_002579C0 */

extern s32 func_00249BB0(s32 id);
extern void func_00249CF0(s32 arg);
extern void func_00249D80(void);
extern void func_002579C0(s32 *tbl);
extern void func_002579E0(void);
extern s32 func_00258038(s32 script);
extern void func_00258D98(void);
extern void func_00259008(void);
extern void func_00259288(void);

/* Returns the battle's shared state. */
BattleWork *Battle_GetWork(void) {
    return &gBattleWork;
}

/* Zeroes the whole work, then puts the setup, result and event blocks in their empty state. */
void Battle_ClearWork(void) {
    memset(Battle_GetWork(), 0, sizeof(BattleWork));
    BattleSetup_Clear();
    BattleResult_Clear();
    BtlEvent_ClearAll();
}

/* Start-of-match reset: stops audio, clears flags/result/events, reloads what changed and starts the BGM. */
void Battle_ResetWork(void) {
    BattleEvents *ev;

    Adx_StopAll();
    Battle_GetWork()->flags = 0;
    BtlLoad_Reload();
    BattleReplay_ClearDataFlag();
    BattleResult_Clear();
    BtlEvent_Reset();
    Bgm_Play(Battle_GetBgm() + 0x10B16);
    if (Battle_GetMode() == 1) {
        ev = Battle_GetEventWork();
        func_00259008();
        ev->unk4 = func_00258038(ev->script);
    }
}

/* Per-frame work update (play time, event bit sets); does nothing while paused. */
void Battle_UpdateWork(void) {
    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    BattleResult_CountFrame();
    BtlEvent_Update();
}

/* Returns the same block as Battle_GetWork (the setup lives at its start). */
BattleSetup *Battle_GetSetup(void) {
    return &Battle_GetWork()->setup;
}

/* Returns the result block. */
BattleResult *Battle_GetResult(void) {
    return &Battle_GetWork()->result;
}

/* Returns the script handle + event bit sets block. */
BattleEvents *Battle_GetEventWork(void) {
    return &Battle_GetWork()->events;
}

/* Returns the opponent pool of mode 3 (work + 0x5A8). */
BattleMemberPool *Battle_GetWork5A8(void) {
    return &Battle_GetWork()->pool;
}

/* Loads everything a battle needs (blocking, behind the loading screen) and creates the two sides' objects. */
s32 Battle_Load(void) {
    func_00249CF0(1);
    func_002579C0(D_002C6EC0);
    func_00258D98();
    BtlLoad_PushInitialJob();
    Load_RunBlocking();
    BattleSide_SetObjId(0, func_00249BB0(BattleSide_GetModelSlot(0)));
    BattleSide_SetObjId(1, func_00249BB0(BattleSide_GetModelSlot(1)));
    return 1;
}

/* Frees everything Battle_Load made. */
s32 Battle_Unload(void) {
    func_00249D80();
    BtlLoad_FreeAll();
    func_00259288();
    func_002579E0();
    return 1;
}
