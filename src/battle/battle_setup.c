#include "common.h"
#include "battle/battle_setup.h"
#include "sys/save.h"

/*
 * Second half of the battle work file: 0x129170..0x12B570 (battle_work.c is 0x126EC8..0x129170 and
 * battle.c starts at 0x12B570). Result finish, the per-side event bits, the battle setup builders
 * and getters, and the replay block. include/battle/battle_setup.h describes the data.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);
extern s32 rand(void);

/* Other modules. The battle work accessors return this file's own views of the blocks. */
/* battle.h declares this one as returning BattleWork *; this file does not include battle.h. */
BtlSetup *Battle_GetSetup(void);            /* the battle work; its first 0x5A8 bytes are the BtlSetup */
BtlResult *Battle_GetResult(void);
BtlEvents *Battle_GetEventWork(void);       /* work + 0x1980 */
BtlMemberPool *Battle_GetWork5A8(void);     /* work + 0x5A8 */
void BattleResult_CollectEvents(BtlResult *res);
BtlClock *BtlSeq_GetClock(void);
BtlClock *BtlSeq_GetSubClock(void);
s32 BtlFacade_AreBothInterruptible(void);  /* BtlCtrl_IsInterruptible(0) && BtlCtrl_IsInterruptible(1): both fighters ready */
void BtlFacade_SetFlag200(void); /* battle flags |= 0x200 */
void BtlFacade_ClearFlag200(void); /* battle flags &= ~0x200 */
void BtlFacade_ClearFixedCamera(void); /* tail call of DemoCam_ClearFixed (camera module) */
void func_00259910(void); /* script module: reacts to events 0x5A / 0x5B */
void func_00219690(f32 t); /* five HUD parts (0x21FAF0, 0x21AD40, 0x22F2F0, 0x22E0B0, 0x2240A0), duration t */
void func_002196D0(f32 t); /* their counterparts (0x21FB18, ...), duration t */
/* Sums the four stat bonuses and ORs the four ability words of the items; stats[4] = AI type. */
void func_00261130(BtlItemSet *items, s32 *stats, s32 *ability, s32 chara);

#define SETUP() Battle_GetSetup()

/*
 * Side and member lookups. The original computes `setup + side * 0x270` first and adds the field offset
 * last, which only comes out when the setup pointer and the element pointer are locals (as here);
 * `SETUP()->sides[n].field` written in one expression adds the operands the other way round.
 */
static inline BtlSide *Side(s32 n) {
    BtlSetup *setup = SETUP();

    return &setup->sides[n];
}

static inline BtlMember *Member(s32 n, s32 i) {
    BtlSide *side = Side(n);

    return &side->members[i];
}

/* Adds one to result.frames on the frame after event 0x48 of side 0 is newly raised. */
void BattleResult_CountFrame(void) {
    BtlResult *res = Battle_GetResult();

    if (BtlEvent_IsNew(0, 0x48)) {
        res->frames++;
    }
}

/* Empty. */
void BattleResult_Stub1291B8(void) {
}

/* Fills the result block at the end of the battle: event summary, clock copy, abort flag, forced winner. */
void BattleResult_Finish(void) {
    BtlResult *res = Battle_GetResult();

    BattleResult_CollectEvents(res);
    res->clock = *BtlSeq_GetClock();
    if (res->flags & 8) {
        res->unk8 = 1;
    }
    if (res->reason & 0x20) {
        res->flags |= 2;
    }
    if (res->reason & 0x40) {
        res->flags |= 1;
    }
}

/* Returns result.flags (winner / draw / abort bits). */
s32 BattleResult_GetFlags(void) {
    return Battle_GetResult()->flags;
}

/* Returns result.reason. */
s32 BattleResult_GetReason(void) {
    return Battle_GetResult()->reason;
}

/* Returns result.eventSummary (64-bit). */
u64 BattleResult_GetEventSummary(void) {
    return Battle_GetResult()->eventSummary;
}

/* Returns the result block (wrapper of Battle_GetResult). */
BtlResult *BattleResult_GetPtr(void) {
    return Battle_GetResult();
}

/* Converts a clock to whole seconds. */
s32 BtlClock_ToSeconds(BtlClock *clock) {
    return clock->hours * 3600 + clock->minutes * 60 + clock->seconds;
}

/* Raises time event i (0..18) on both sides every frame once the sub clock has reached tbl[i] seconds. */
void BtlEvent_RaiseTimeEvents(void) {
    s32 tbl[19] = { 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, 90, 100, 120, 140, 160, 180 };
    s32 sec;
    u32 i;

    sec = BtlClock_ToSeconds(BtlSeq_GetSubClock());
    for (i = 0; i < 19; i++) {
        if (sec >= tbl[i]) {
            BtlEvent_Raise(0, i);
            BtlEvent_Raise(1, i);
        }
    }
}

/* Per-frame handling of the pending interrupt (event 0x4C), the wait flag (events 0x4D / 0x4E) and event 0x4F. */
void BtlEvent_UpdateRequests(void) {
    BtlEvents *ev = Battle_GetEventWork();
    s32 i;

    if (ev->interrupt != 0) {
        if (BtlFacade_AreBothInterruptible() != 0) {
            BtlEvent_Raise(0, 0x4C);
            BtlEvent_Raise(1, 0x4C);
            BtlFacade_ClearFlag200();
            ev->interrupt = 0;
        }
    }
    if (ev->waitFlag == 1) {
        if (BtlEvent_IsNew(0, 0x4D) || BtlEvent_IsNew(1, 0x4D) || BtlEvent_IsNew(0, 0x4E) || BtlEvent_IsNew(1, 0x4E)) {
            ev->waitFlag = 0;
        }
    }
    for (i = 0; i < 2; i++) {
        if (BtlEvent_IsNew(i, 0x4F)) {
            BtlFacade_ClearFixedCamera();
        }
    }
}

/* End of frame for one side: prev = now, held |= now, now = 0. */
void BtlEventSet_Rotate(BtlEventSet *set) {
    s32 i;

    for (i = 0; i < 2; i++) {
        set->prev[i] = set->now[i];
        set->held[i] |= set->now[i];
        set->now[i] = 0;
    }
}

/* Sets bit n of a 64-bit word array. */
void BtlEvent_SetBit(u64 *bits, s32 n) {
    u64 mask = 1;

    bits += n / 64;
    *bits |= mask << (n % 64);
}

/* 1 when event n is raised in `now` and was not in `prev`. */
s32 BtlEventSet_IsNew(BtlEventSet *set, s32 n) {
    return ((set->now[n / 64] & ~set->prev[n / 64]) >> (n % 64)) & 1;
}

/* 1 when event n was raised at any time since the last reset. */
s32 BtlEventSet_WasRaised(BtlEventSet *set, s32 n) {
    return (set->held[n / 64] >> (n % 64)) & 1;
}

/* Zeroes the whole event work. */
void BtlEvent_ClearAll(void) {
    memset(Battle_GetEventWork(), 0, sizeof(BtlEvents));
}

/* Zeroes the event work but keeps its first word (the script handle). */
void BtlEvent_Reset(void) {
    s32 keep = Battle_GetEventWork()->script;

    BtlEvent_ClearAll();
    Battle_GetEventWork()->script = keep;
}

/* Raises event ev for a side; events 0x5A and 0x5B also notify the script module. */
void BtlEvent_Raise(s32 side, s32 ev) {
    BtlEvent_SetBit(Battle_GetEventWork()->set[side].now, ev);
    switch (ev) {
    case 0x5A:
    case 0x5B:
        func_00259910();
        break;
    }
}

/* BtlEventSet_IsNew on a side. */
s32 BtlEvent_IsNew(s32 side, s32 ev) {
    return BtlEventSet_IsNew(&Battle_GetEventWork()->set[side], ev);
}

/* BtlEventSet_WasRaised on a side. */
s32 BtlEvent_WasRaised(s32 side, s32 ev) {
    return BtlEventSet_WasRaised(&Battle_GetEventWork()->set[side], ev);
}

/* Once per unpaused frame: requests, rotate both sides, then raise the time events for the new frame. */
void BtlEvent_Update(void) {
    BtlEvents *ev = Battle_GetEventWork();
    s32 i;

    BtlEvent_UpdateRequests();
    for (i = 0; i < 2; i++) {
        BtlEventSet_Rotate(&ev->set[i]);
    }
    BtlEvent_RaiseTimeEvents();
}

/* Asks for event 0x4C: sets the pending word and battle flag 0x200, and fades the HUD out over 1.0. */
void BtlEvent_BeginInterrupt(void) {
    Battle_GetEventWork()->interrupt = 1;
    BtlFacade_SetFlag200();
    func_00219690(1.0f);
}

/* Fades the HUD back in over 1.0. */
void BtlEvent_EndInterrupt(void) {
    func_002196D0(1.0f);
}

/* Stores the wait flag: waitFlag = (off == 0). */
void BtlEvent_SetWaitOff(s32 off) {
    Battle_GetEventWork()->waitFlag = off == 0;
}

/* 1 when the wait flag is clear. */
s32 BtlEvent_IsWaitOff(void) {
    return Battle_GetEventWork()->waitFlag == 0;
}

/* Clamps the eight bonus levels of a member to -20..20, or -20..60 when wide. */
void BtlMember_ClampBonus(BtlMember *m, s32 wide) {
    s32 min;
    s32 max;

    if (wide != 0) {
        min = -20;
        max = 60;
    } else {
        min = -20;
        max = 20;
    }
    if (m->bonus[0] > max) {
        m->bonus[0] = max;
    } else if (m->bonus[0] < -20) {
        m->bonus[0] = min;
    }
    if (m->bonus[1] > max) {
        m->bonus[1] = max;
    } else if (m->bonus[1] < -20) {
        m->bonus[1] = min;
    }
    if (m->bonus[2] > max) {
        m->bonus[2] = max;
    } else if (m->bonus[2] < -20) {
        m->bonus[2] = min;
    }
    if (m->bonus[3] > max) {
        m->bonus[3] = max;
    } else if (m->bonus[3] < -20) {
        m->bonus[3] = min;
    }
    if (m->bonus[4] > max) {
        m->bonus[4] = max;
    } else if (m->bonus[4] < -20) {
        m->bonus[4] = min;
    }
    if (m->bonus[5] > max) {
        m->bonus[5] = max;
    } else if (m->bonus[5] < -20) {
        m->bonus[5] = min;
    }
    if (m->bonus[6] > max) {
        m->bonus[6] = max;
    } else if (m->bonus[6] < -20) {
        m->bonus[6] = min;
    }
    if (m->bonus[7] > max) {
        m->bonus[7] = max;
    } else if (m->bonus[7] < -20) {
        m->bonus[7] = min;
    }
}

/* Recomputes bonus levels, AI type and ability bits of a member from its equipped items. */
void BtlMember_ApplyItems(BtlMember *m) {
    s32 ability[4];
    s32 stats[5];
    s32 i;

    func_00261130(&m->items, stats, ability, m->chara);
    m->bonus[0] = 0;
    m->bonus[1] = stats[2];
    m->bonus[2] = stats[0];
    m->bonus[3] = stats[1];
    m->bonus[4] = stats[2];
    m->bonus[5] = stats[3];
    m->bonus[6] = stats[3];
    m->bonus[7] = stats[3];
    m->aiType = stats[4];
    for (i = 0; i < 4; i++) {
        m->ability[i] = ability[i];
    }
}

/*
 * Clears a member and fills it: items (optional), character, costume, variant, CPU level, health.
 * BtlMember_ApplyItems runs while m->chara is still 0, so without an AI item the AI type is the one of
 * character 0, whatever `chara` is.
 */
void BtlMember_Init(BtlMember *m, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                    BtlItemSet *items) {
    memset(m, 0, sizeof(BtlMember));
    if (items != NULL) {
        m->items = *items;
    }
    BtlMember_ApplyItems(m);
    m->chara = chara;
    m->costume = costume;
    m->variant = variant;
    m->cpuLevel = cpuLevel;
    m->health = health;
}

/* Forces the settings a mode does not allow (split-screen, time limit, team size, who is CPU). */
void BattleSetup_FixForMode(void) {
    BtlRule *rule = &SETUP()->rule;
    BtlSide *side;
    s32 i;
    s32 j;

    if (Battle_GetMode() != 0 && Battle_GetMode() != 4) {
        if (rule->screenMode == 1) {
            rule->screenMode = 0;
        }
    }
    if (Battle_GetMode() == 9) {
        if (rule->timeLimit != 0) {
            rule->timeLimit = 0;
        }
    }
    if (Battle_GetMode() == 7) {
        if (rule->timeLimit != 5) {
            rule->timeLimit = 5;
        }
    }
    for (i = 0; i < 2; i++) {
        side = &SETUP()->sides[i];
        switch (Battle_GetMode()) {
        case 2:
            break;
        case 9:
            if (side->control != 2) {
                side->control = 2;
                for (j = 0; j < 5; j++) {
                    BtlMember *m = &SETUP()->sides[i].members[j];

                    m->cpuLevel = 29;
                    m->aiType = 0;
                }
            }
            break;
        case 6:
            if (side->memberCount != 1) {
                side->memberCount = 1;
            }
            if (i == 0) {
                if (side->control != 0) {
                    side->control = 0;
                }
            }
            if (i == 1) {
                if (side->control != 2) {
                    side->control = 2;
                }
                if (side->members[0].cpuLevel != -1) {
                    side->members[0].cpuLevel = -1;
                }
            }
            break;
        case 4:
            if (side->memberCount != 1) {
                side->memberCount = 1;
            }
            break;
        case 3:
            if (i == 0) {
                if (side->memberCount != 1) {
                    side->memberCount = 1;
                }
                if (side->control != 0) {
                    side->control = 0;
                }
            }
            if (i == 1) {
                if (side->control != 2) {
                    side->control = 2;
                }
            }
            break;
        case 7:
            if (side->control != 2) {
                side->control = 2;
            }
            break;
        }
    }
    if (Battle_GetMode() == 9) {
        SETUP()->rule.mode = 6;
    }
}

/* Sets a side's usable-character bits: a copy of src, or the save's unlocked characters. */
void BattleSetup_InitCharaBits(BtlCharaBits *dst, BtlCharaBits *src) {
    if (src != NULL) {
        *dst = *src;
    } else {
        memset(dst, 0, sizeof(BtlCharaBits));
        dst->bits[0] = gSaveData->charaBits[0];
        dst->bits[1] = gSaveData->charaBits[1];
        dst->bits[2] = gSaveData->charaBits[2];
    }
}

/* Unless options were given, takes the two per-side options from the save data. */
void BattleSetup_DefaultOptions(void) {
    BtlOption *opt = &SETUP()->option;

    if (opt->isSet == 0) {
        memset(opt, 0, sizeof(BtlOption));
        opt->optA[0] = gSaveData->unk1694;
        opt->optA[1] = gSaveData->unk1694;
        opt->optB[0] = gSaveData->unk1698;
        opt->optB[1] = gSaveData->unk1698;
    }
}

/* Zeroes the setup and writes its magic "btls" and version 7. */
void BattleSetup_Clear(void) {
    memset(SETUP(), 0, sizeof(BtlSetup));
    SETUP()->magic[0] = 'b';
    SETUP()->magic[1] = 't';
    SETUP()->magic[2] = 'l';
    SETUP()->magic[3] = 's';
    SETUP()->version = 7;
}

/* Clears bit 0 of the replay data flags. */
void BattleReplay_ClearDataFlag(void) {
    BattleReplay_GetData()->flags &= ~1;
}

/* Selects battle script n (stored + 1; 0 = no script). */
void BattleSetup_SetScript(s32 script) {
    SETUP()->script = script + 1;
}

/* Sets the two per-side options explicitly. */
void BattleSetup_SetOptions(s32 optA0, s32 optA1, s32 optB0, s32 optB1) {
    BtlOption *opt = &SETUP()->option;

    memset(opt, 0, sizeof(BtlOption));
    opt->isSet = 1;
    opt->optA[0] = optA0;
    opt->optA[1] = optA1;
    opt->optB[0] = optB0;
    opt->optB[1] = optB1;
}

/* Sets option word 0x14 and takes the per-side options from the save data. */
void BattleSetup_SetOption14(s32 val) {
    BtlOption *opt = &SETUP()->option;

    memset(opt, 0, sizeof(BtlOption));
    opt->isSet = 1;
    opt->unk14 = val;
    opt->optA[0] = gSaveData->unk1694;
    opt->optA[1] = gSaveData->unk1694;
    opt->optB[0] = gSaveData->unk1698;
    opt->optB[1] = gSaveData->unk1698;
}

/* Sets the battle rules; bgm 24 means a random one of 0..23. */
void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10) {
    BtlRule *rule = &SETUP()->rule;

    rule->screenMode = screenMode;
    rule->mode = mode;
    if (bgm == 24) {
        rule->bgm = rand() % 24;
    } else {
        rule->bgm = bgm;
    }
    rule->timeLimit = timeLimit;
    rule->announcer = announcer;
    rule->unk10 = unk10;
    rule->stage = stage;
    rule->curStage = stage;
}

/* Sets one side: who controls it, pad, team size, options, lead member and usable characters. */
void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead,
                   BtlCharaBits *bits) {
    BtlSide *side = Side(sideNo);

    side->control = control;
    side->pad = pad;
    side->memberCount = memberCount;
    side->unk200 = unk200;
    side->unk1FC = unk1FC;
    side->lead = lead;
    BattleSetup_InitCharaBits(&side->charaBits, bits);
}

/* Sets one team member from 0-based item ids stored in 32-bit words. */
void BattleSetup_SetMemberByItemIds(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel,
                                    f32 health, u32 *itemIds) {
    BtlMember *m = Member(sideNo, idx);
    BtlItemSet items;
    s32 i;

    memset(&items, 0, sizeof(items));
    for (i = 0; i < 8; i++) {
        items.id[i] = *(u16 *)&itemIds[i] + 1;
    }
    BtlMember_Init(m, chara, costume, variant, cpuLevel, health, &items);
}

/* Sets one team member (items: 1-based u16 ids or NULL). */
void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                           BtlItemSet *items) {
    BtlMember_Init(Member(sideNo, idx), chara, costume, variant, cpuLevel, health, items);
}

/* Sets entry idx of the opponent pool and the pool size. */
void BattleSetup_SetPoolMember(s32 count, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health,
                               BtlItemSet *items) {
    BtlMemberPool *pool = Battle_GetWork5A8();

    pool->count = count;
    pool->cur = 0;
    BtlMember_Init(&pool->members[idx], chara, costume, variant, cpuLevel, health, items);
}

/* BattleSetup_FinishEx(0). */
void BattleSetup_Finish(void) {
    BattleSetup_FinishEx(0);
}

/* Last step of a setup: mode fix-ups, lead character, bonus clamps, rule.unk18, options, replay copy. */
void BattleSetup_FinishEx(s32 wide) {
    s32 cpuWide;
    s32 i;
    s32 j;
    BtlSide *side;
    BtlMember *m;
    BtlRule *rule;
    BtlSetup *setup;
    BtlMemberPool *pool;

    BattleSetup_FixForMode();
    switch (Battle_GetMode()) {
    case 1:
    case 2:
    case 3:
    case 4:
        cpuWide = 1;
        break;
    default:
        cpuWide = 0;
        break;
    }
    if (Battle_GetMode() == 3) {
        pool = Battle_GetWork5A8();
        pool->cur = 0;
        for (j = 0; j < 50; j++) {
            BtlMember_ClampBonus(&pool->members[j], 1);
        }
        SETUP()->sides[1].members[0] = pool->members[0];
    }
    for (i = 0; i < 2; i++) {
        setup = SETUP();
        side = &setup->sides[i];
        side->startForm.chara = side->members[side->lead].chara;
        side->startForm.costume = side->members[side->lead].costume;
        side->startForm.variant = side->members[side->lead].variant;
        side->form.chara = side->startForm.chara;
        side->form.costume = side->startForm.costume;
        side->form.variant = side->startForm.variant;
        switch (Battle_GetMode()) {
        case 2:
        case 3:
        case 4:
        case 7:
        case 9:
            if (side->control != 2) {
                break;
            }
        case 1:
            side->charaBits.bits[0] = -1;
            side->charaBits.bits[1] = -1;
            side->charaBits.bits[2] = -1;
            break;
        case 5:
        case 6:
        case 8:
            break;
        }
        for (j = 0; j < 5; j++) {
            m = &SETUP()->sides[i].members[j];
            if (side->control == 2) {
                if (cpuWide) {
                    BtlMember_ClampBonus(m, 1);
                } else {
                    BtlMember_ClampBonus(m, 0);
                }
            } else if (wide) {
                BtlMember_ClampBonus(m, 1);
            } else {
                BtlMember_ClampBonus(m, 0);
            }
        }
    }
    rule = &SETUP()->rule;
    rule->unk18 = 0;
    if (Battle_GetMode() == 1) {
        for (i = 0; i < 7; i++) {
            s32 mask = 1 << i;

            if (!(gSaveData->unlockFlags & mask)) {
                rule->unk18 = i + 1;
                break;
            }
        }
    }
    BattleSetup_DefaultOptions();
    SETUP()->unk5A0 = 1;
    memset(&gBattleReplay, 0, sizeof(BtlReplay));
    gBattleReplay.setup = *SETUP();
}

/* Returns the replay block (and its size) for saving; clears its two trailing words. */
BtlReplay *BattleReplay_GetBuffer(s32 *size) {
    BtlReplay *rep;

    if (size != NULL) {
        *size = sizeof(BtlReplay);
    }
    rep = &gBattleReplay;
    rep->loaded = 0;
    rep->unk1ABA4 = 0;
    return rep;
}

/* Installs a loaded replay block: its setup becomes the battle setup. Returns 1 when the block is valid. */
s32 BattleReplay_Load(BtlReplay *buf, s32 size) {
    if (buf != NULL) {
        if (size == sizeof(BtlReplay)) {
            if (*(u64 *)buf == (((u64)BTL_SETUP_VERSION << 32) | BTL_SETUP_MAGIC)) {
                *SETUP() = buf->setup;
                memset(&gBattleReplay, 0, sizeof(BtlReplay));
                gBattleReplay.setup = buf->setup;
                memcpy(&gBattleReplay.data, &buf->data, sizeof(BtlReplayData) + sizeof(s32));
                gBattleReplay.active = 1;
                gBattleReplay.loaded = 1;
                return 1;
            }
        }
    }
    return 0;
}

/* Non-zero while the battle plays a replay. */
s32 BattleReplay_IsActive(void) {
    return gBattleReplay.active;
}

/* Non-zero when the replay block came from BattleReplay_Load. */
s32 BattleReplay_IsLoaded(void) {
    return gBattleReplay.loaded;
}

/* Returns the recorded data part of the replay block. */
BtlReplayRec *BattleReplay_GetData(void) {
    return (BtlReplayRec *)&gBattleReplay.data;
}

/* 1 when no replay is active, else bit 0 of the replay data flags. */
s32 BattleReplay_TestDataFlag(void) {
    BtlReplayRec *data = BattleReplay_GetData();

    if (BattleReplay_IsActive() == 0) {
        return 1;
    }
    if ((data->flags & 1) != 0) {
        return 1;
    }
    return 0;
}

/* Sets the replay-active word. */
void BattleReplay_SetActive(s32 active) {
    gBattleReplay.active = active;
}

/* First side whose control is 0 (a pad), or -1. */
s32 Battle_GetHumanSide(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (Side(i)->control == 0) {
            return i;
        }
    }
    return -1;
}

/* Returns option word 0x14. */
s32 Battle_GetOption14(void) {
    return SETUP()->option.unk14;
}

/* Returns the battle script number, -1 when there is none. */
s32 Battle_GetScript(void) {
    return SETUP()->script - 1;
}

/* 1 when the battle is drawn in split-screen. */
s32 Battle_IsSplitScreen(void) {
    return SETUP()->rule.screenMode == 1;
}

/* Returns the battle mode (BATTLE_MODE_*). */
s32 Battle_GetMode(void) {
    return SETUP()->rule.mode;
}

/* Time limit in seconds (-1 = none). */
s32 Battle_GetTimeLimit(void) {
    return gBattleTimeLimitTbl[SETUP()->rule.timeLimit];
}

/* 1 when the time limit setting is 0 (no limit). */
s32 Battle_IsTimeLimitOff(void) {
    return SETUP()->rule.timeLimit == 0;
}

/* Returns the announcer voice (0..7). */
s32 Battle_GetAnnouncer(void) {
    return SETUP()->rule.announcer;
}

/* Returns the BGM number. */
s32 Battle_GetBgm(void) {
    return SETUP()->rule.bgm;
}

/* Returns the stage the battle starts on. */
s32 Battle_GetStartStage(void) {
    return SETUP()->rule.stage;
}

/* Returns the current stage. */
s32 Battle_GetStage(void) {
    return SETUP()->rule.curStage;
}

/* Sets the current stage. */
void Battle_SetStage(s32 stage) {
    SETUP()->rule.curStage = stage;
}

/* 1 when the current stage is not the starting one. */
s32 Battle_IsStageChanged(void) {
    return Battle_GetStartStage() != Battle_GetStage();
}

/* Current stage = starting stage. */
void Battle_ResetStage(void) {
    BtlRule *rule = &SETUP()->rule;

    rule->curStage = SETUP()->rule.stage;
}

/* Returns rule word 0x10 (setup + 0x18). */
s32 Battle_GetRuleUnk10(void) {
    return SETUP()->rule.unk10;
}

/* Per-side option A (save 0x1694 by default). */
s32 BattleSide_GetOptionA(s32 side) {
    s32 *p = SETUP()->option.optA;

    return p[side];
}

/* Per-side option B (save 0x1698 by default). */
s32 BattleSide_GetOptionB(s32 side) {
    s32 *p = SETUP()->option.optB;

    return p[side];
}

/* Returns the side's controller number. */
s32 BattleSide_GetPad(s32 side) {
    return Side(side)->pad;
}

/* Returns who controls the side (0 pad, 2 CPU). */
s32 BattleSide_GetControl(s32 side) {
    return Side(side)->control;
}

/* 1 when the side is CPU controlled. */
s32 BattleSide_IsCpu(s32 side) {
    return Side(side)->control == 2;
}

/* Returns side word 0x200. */
s32 BattleSide_GetUnk200(s32 side) {
    return Side(side)->unk200;
}

/* Returns side word 0x1FC. */
s32 BattleSide_GetUnk1FC(s32 side) {
    return Side(side)->unk1FC;
}

/* Character the side starts the battle with. */
s32 BattleSide_GetStartChara(s32 side) {
    return Side(side)->startForm.chara;
}

/* Costume the side starts the battle with. */
s32 BattleSide_GetStartCostume(s32 side) {
    return Side(side)->startForm.costume;
}

/* Model variant the side starts the battle with. */
s32 BattleSide_GetStartVariant(s32 side) {
    return Side(side)->startForm.variant;
}

/* Character currently loaded for the side. */
s32 BattleSide_GetChara(s32 side) {
    return Side(side)->form.chara;
}

/* Sets the character currently loaded for the side. */
void BattleSide_SetChara(s32 side, s32 chara) {
    Side(side)->form.chara = chara;
}

/* 1 when the current character is not the starting one. */
s32 BattleSide_IsCharaChanged(s32 side) {
    return BattleSide_GetStartChara(side) != BattleSide_GetChara(side);
}

/* Current character = starting character. */
void BattleSide_ResetChara(s32 side) {
    BattleSide_SetChara(side, BattleSide_GetStartChara(side));
}

/* Character of team member idx. */
s32 BattleSide_GetMemberChara(s32 side, s32 idx) {
    return Member(side, idx)->chara;
}

/* Costume of team member idx. */
s32 BattleSide_GetMemberCostume(s32 side, s32 idx) {
    return Member(side, idx)->costume;
}

/* Model variant of team member idx. */
s32 BattleSide_GetMemberVariant(s32 side, s32 idx) {
    return Member(side, idx)->variant;
}

/* Team size of the side. */
s32 BattleSide_GetMemberCount(s32 side) {
    return Side(side)->memberCount;
}

/* Object id of the side's fighter (argument of BtlObj_Get). */
s32 BattleSide_GetObjId(s32 side) {
    return Side(side)->objId;
}

/* Returns side word 0x26C (model slot from func_00249C60). */
s32 BattleSide_GetModelSlot(s32 side) {
    return Side(side)->modelSlot;
}

/* Sets the object id of the side's fighter. */
void BattleSide_SetObjId(s32 side, s32 objId) {
    Side(side)->objId = objId;
}

/* Sets side word 0x26C. */
void BattleSide_SetModelSlot(s32 side, s32 slot) {
    Side(side)->modelSlot = slot;
}

/* Sets the currently loaded character, costume and variant. */
void BattleSide_SetForm(s32 side, s32 chara, s32 costume, s32 variant) {
    Side(side)->form.chara = chara;
    Side(side)->form.costume = costume;
    Side(side)->form.variant = variant;
}

/* 1 when the current character, costume or variant differs from the starting one. */
s32 BattleSide_IsFormChanged(s32 side) {
    if (Side(side)->form.chara != Side(side)->startForm.chara) {
        return 1;
    }
    if (Side(side)->form.costume != Side(side)->startForm.costume) {
        return 1;
    }
    return Side(side)->form.variant != Side(side)->startForm.variant;
}

/* Returns team member idx of a side. */
BtlMember *BattleSide_GetMember(s32 side, s32 idx) {
    return Member(side, idx);
}

/* Replaces a member's items and recomputes what derives from them; nothing when items is NULL. */
void BattleSide_SetMemberItems(s32 side, s32 idx, BtlItemSet *items) {
    BtlMember *m = BattleSide_GetMember(side, idx);

    if (items != NULL) {
        m->items = *items;
        BtlMember_ApplyItems(m);
    }
}

/* 1 when character id is in the side's usable-character bits. */
s32 BattleSide_IsCharaUsable(s32 side, s32 chara) {
    u64 *bits = Side(side)->charaBits.bits;

    return (bits[chara / 64] >> (chara % 64)) & 1;
}
