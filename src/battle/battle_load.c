#include "common.h"
#include "battle/battle_work.h"
#include "sys/adx.h"
#include "sys/common.h"
#include "sys/file.h"
#include "sys/heap.h"
#include "sys/job.h"
#include "sys/loading.h"

/* Battle loader (job pool, job step functions, load requests) and result accessors: 0x127120..0x129170.
 * What a load does step by step is described in include/battle/battle_work.h.
 *
 * The battle work accessors at 0x126EC8..0x127120 are a separate translation unit (src/battle/battle_work.c):
 * with Battle_GetWork or Battle_GetEventWork defined earlier in this file, ee-gcc 2.96 fills the branch delay
 * slots of BtlLoad_StepStageReload / StepObject / StepChara / StepInitial differently and they do not match. */

extern void *memset(void *dst, s32 c, u32 n);

extern BtlJobPool gBtlJobPool;
extern AdxSaveView *gSaveData;
extern s32 gBtlLoadHandle; /* handle of the model BtlLoad_StepObject is loading */
extern s32 gBtlLoadObj;    /* object made from it */

/* sys/file.h declares File_Request with two parameters (it ignores a third); every caller here passes the
 * buffer size as a third argument, so the calls go through a cast. */
#define File_Request3(id, buf, size) ((void *(*)(s32, void *, s32))File_Request)(id, buf, size)

extern void Fade_Start(s32 idx, s32 dir, f32 seconds);
extern s32 Fade_IsDone(s32 idx);
extern void Snd_LoadBank(s32 mask, void *data, s32 arg);
extern void Snd_UnloadBank(s32 mask);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern s32 BtlObj_Get(s32 id);

/* Sound driver. */
extern void Snd_Reset(void);                         /* clears the 8 bank slots' transfer state */
extern void Snd_ReloadBank(s32 mask, void *data, s32 arg); /* reloads a bank from a buffer that stays allocated */
extern s32 Snd_IsUploadDone(void);                          /* 1 when no bank slot has a transfer pending */

/* Battle work / setup accessors that follow this file (0x129170..0x12B570). */
extern void BattleResult_CountFrame(void);
extern void BtlEvent_ClearAll(void);
extern void BtlEvent_Reset(void);
extern s32 BtlEvent_WasRaised(s32 set, s32 bit);  /* event bit `bit` of set `set` */
extern void BtlEvent_Update(void);
extern void BattleSetup_Clear(void);
extern void BattleReplay_ClearDataFlag(void);
extern void BattleSetup_Finish(void);
extern void BattleReplay_SetActive(s32 arg);
extern s32 Battle_GetScript(void);              /* work->unkBC - 1 */
extern s32 Battle_GetBgm(void);              /* work->bgm */
extern s32 Battle_GetStartStage(void);              /* work->unk1C: the stage the battle was set up with */
extern s32 Battle_GetStage(void);              /* work->unk28: the stage currently loaded */
extern void Battle_SetStage(s32 stage);        /* work->unk28 = stage */
extern s32 BattleSide_GetStartChara(s32 side);          /* side->unk250[0]: initial character */
extern s32 BattleSide_GetStartCostume(s32 side);          /* side->unk250[1]: initial costume */
extern s32 BattleSide_GetStartVariant(s32 side);          /* side->unk250[2] */
extern u32 BattleSide_GetMemberCount(s32 side);          /* side->unk0: members in use */
extern s32 BattleSide_GetObjId(s32 side);          /* side->objId */
extern s32 BattleSide_GetModelSlot(s32 side);          /* side->unk26C */
extern void BattleSide_SetObjId(s32 side, s32 id); /* side->objId = id */
extern void BattleSide_SetModelSlot(s32 side, s32 id); /* side->unk26C = id */
extern void BattleSide_SetForm(s32 side, s32 chara, s32 costume, s32 variant); /* side->unk25C[] = current triple */
extern s32 BattleSide_IsFormChanged(s32 side);          /* current triple != initial triple */
extern BtlMemberView *BattleSide_GetMember(s32 side, s32 member);
extern void BtlScene_FreeChar(s32 side);
extern void BtlScene_CreateChar(s32 side);

/* Stage / scene. */
extern void func_00115170(void);
extern void func_00137BD0(void);
extern void func_00137BF8(void);
extern void func_0013F310(void);
extern s32 func_0013F3A0(void);
extern void func_0013F3C8(void);
extern void func_001B3628(void);
extern void func_0023FCD8(void);
extern void func_002473C8(s32 arg);
extern void func_002473D8(void);

/* Fighters. */
extern void func_001BB1F0(s32 side);
extern void func_001C29A0(s32 side);
extern void func_001C29D8(void);
extern s32 func_0020B200(s32 side); /* pending request of type 0 for this side */
extern s32 func_0020B248(s32 side); /* pending request of type 1 for this side */
extern void func_0020B290(s32 *a, s32 *b, s32 *c, s32 *d, s32 *e, s32 *f, s32 *g); /* request words +8..+0x20 */
extern void func_0020B338(void);    /* request taken */
extern void func_0020B350(void);    /* request's files are in */
extern s32 func_0020B368(void);     /* request state == 4 and not paused */
extern void func_0020B3C0(s32 side, s32 handle, s32 obj);
extern s32 func_0020B5E8(s32 side); /* member index */
extern s32 func_0020BEC8(s32 side);

/* Battle objects / models. */
extern s32 func_00249AB8(s32 slot, s32 model, s32 arg);
extern s32 func_00249BB0(s32 id);
extern void func_00249BD8(s32 objId, s32 id);
extern s32 func_00249C60(s32 side, s32 chara, s32 costume, s32 variant);
extern void func_00249CD8(void);
extern void func_00249CF0(s32 arg);
extern void func_00249D80(void);
extern s32 func_0024B7A8(s32 arg, s32 file, s32 file8, s32 file9);
extern s32 func_0024B910(s32 handle);
extern void func_0024B9A8(s32 id, s32 file, s32 file8, s32 file9);
extern void func_0024BAC0(void);
extern void func_0024D330(s32 obj, s32 arg, s32 arg2);
extern void func_0024D390(s32 obj, s32 arg, s32 arg2);

/* Script / message objects. */
extern void func_002579C0(s32 *tbl);
extern void func_002579E0(void);
extern s32 func_00257DA0(void *data);
extern void func_00257E80(s32 script);
extern s32 func_00258038(s32 script);
extern void func_00258050(s32 script, s32 arg);
extern void func_00258D98(void);
extern void func_00258DB0(void);
extern void func_00258DB8(s32 script);
extern void func_00259008(void);
extern void func_00259288(void);

#define BATTLE_RES ((BattleRes *)((u8 *)gCommonRes + 0x20))

/* Returns the loader job pool. */
BtlJobPool *BtlJob_GetPool(void) {
    return &gBtlJobPool;
}

/* Clears the job pool and puts all 8 jobs on its free list. */
void BtlJob_InitPool(void) {
    BtlJobPool *pool = BtlJob_GetPool();
    s32 i;

    memset(pool, 0, sizeof(BtlJobPool));
    for (i = 0; i < BTL_JOB_COUNT; i++) {
        SList_PushFront(&pool->free, &pool->jobs[i].node);
    }
}

/* Takes a zeroed job from the pool; NULL when all 8 are in use. */
BtlJob *BtlJob_Alloc(void) {
    BtlJob *job = (BtlJob *)SList_PopFront(&BtlJob_GetPool()->free);

    if (job == NULL) {
        return NULL;
    }
    memset(job, 0, sizeof(BtlJob));
    return job;
}

/* Gives a job back to the pool. */
s32 BtlJob_Free(BtlJob *job) {
    BtlJobPool *pool = BtlJob_GetPool();

    if (job != NULL) {
        SList_PushFront(&pool->free, &job->node);
        return 1;
    }
    return 0;
}

/* Number of jobs not in use. */
s32 BtlJob_GetFreeCount(void) {
    return SList_GetCount(&BtlJob_GetPool()->free);
}

/* Shuts the stage-dependent systems down before the stage file is replaced. */
void BtlLoad_BeginStageSwap(void) {
    func_00137BF8();
    func_0023FCD8();
    func_002473C8(1);
}

/* Brings them back up on the new stage. */
void BtlLoad_EndStageSwap(void) {
    func_00115170();
    func_001C29D8();
    func_001B3628();
    func_00137BD0();
    func_002473D8();
}

/* Job: reloads the stage model and stage sound bank (no transition). */
s32 BtlLoad_StepStageReload(BtlJob *job) {
    BattleRes *res = BATTLE_RES;
    s32 id;

    switch (job->state) {
    case 0:
        Battle_GetWork()->flags |= BATTLE_FLAG_LOADING;
        job->state++;
        break;
    case 1:
        BtlLoad_BeginStageSwap();
        if (Battle_IsSplitScreen()) {
            id = Battle_GetStage() + BTL_FILE_STAGE_SPLIT;
        } else {
            id = Battle_GetStage() + BTL_FILE_STAGE;
        }
        res->stage = File_Request3(id, res->stage, res->stageSize);
        res->bank = File_Request3(Battle_GetStage() + BTL_FILE_SND_STAGE, res->bank, res->bankSize);
        job->state++;
        return 0;
    case 2:
        if (!File_UpdateRequests()) {
            return 0;
        }
        Snd_ReloadBank(8, res->bank, 0);
        job->state++;
        break;
    case 3:
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
            return 0;
        }
        Battle_GetWork()->flags &= ~BATTLE_FLAG_LOADING;
        BtlLoad_EndStageSwap();
        BtlJob_Free(job);
        return 1;
    default:
        return 1;
    }
    return 0;
}

/* Job: in-battle stage change, with the transition scene and a fade out / in. */
s32 BtlLoad_StepStageChange(BtlJob *job) {
    BattleRes *res = BATTLE_RES;

    switch (job->state) {
    case 0:
        Battle_GetWork()->flags |= BATTLE_FLAG_LOADING;
        job->state++;
        break;
    case 1:
        if (job->kind == 3) {
            res->transition = File_Request3(BTL_FILE_TRANSITION_3, NULL, 0);
        } else {
            res->transition = File_Request3(BTL_FILE_TRANSITION, NULL, 0);
        }
        BtlLoad_BeginStageSwap();
        job->state++;
        break;
    case 2:
        if (!File_UpdateRequests()) {
            return 0;
        }
        job->state++;
        break;
    case 3:
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
            return 0;
        }
        func_0013F310();
        job->state++;
        return 0;
    case 4:
        res->stage = File_Request3(Battle_GetStage() + BTL_FILE_STAGE, res->stage, res->stageSize);
        res->bank = File_Request3(Battle_GetStage() + BTL_FILE_SND_STAGE, res->bank, res->bankSize);
        job->state++;
        return 0;
    case 5:
        if (!File_UpdateRequests()) {
            return 0;
        }
        Snd_ReloadBank(8, res->bank, 0);
        job->state++;
        /* fall through */
    case 6:
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
            return 0;
        }
        if (func_0013F3A0()) {
            return 0;
        }
        Fade_Start(2, 0, 1.0f);
        job->state++;
        break;
    case 7:
        if (Fade_IsDone(2)) {
            func_0013F3C8();
            job->state++;
        }
        break;
    case 8:
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
            return 0;
        }
        Battle_GetWork()->flags &= ~BATTLE_FLAG_LOADING;
        BtlLoad_EndStageSwap();
        if (res->transition != NULL) {
            Heap_Free(res->transition);
            res->transition = NULL;
        }
        job->state++;
        break;
    case 9:
        Fade_Start(2, 1, 1.0f);
        BtlJob_Free(job);
        return 1;
    default:
        return 1;
    }
    return 0;
}

/* Job: loads one extra model and makes a battle object from it. */
s32 BtlLoad_StepObject(BtlJob *job) {
    s32 id;
    s32 ready;

    switch (job->state) {
    case 0:
        Battle_GetWork()->flags |= BATTLE_FLAG_UNK1000;
        id = job->chara + BTL_FILE_OBJECT;
        if (job->chara < 0x100) {
            if (job->unk1C != 0) {
                id = job->chara * 10 + job->animChara + (BTL_FILE_CHARA + 4);
            } else {
                id = job->chara * 10 + job->animChara + BTL_FILE_CHARA;
            }
        }
        gBtlLoadHandle = func_0024B7A8(0, id, -1, -1);
        job->state++;
        return 0;
    case 1:
        if (!File_UpdateRequests()) {
            return 0;
        }
        switch (job->kind) {
        case 0:
            break;
        case BTL_JOB_KIND_OBJECT:
            func_0020B350();
            break;
        }
        job->state++;
        break;
    case 2:
        ready = 1;
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
            return 0;
        }
        switch (job->kind) {
        case 0:
            break;
        case BTL_JOB_KIND_OBJECT:
            ready = func_0020B368();
            break;
        }
        if (!ready) {
            return 0;
        }
        switch (job->kind) {
        case 0:
            gBtlLoadObj = func_00249AB8(2, func_0024B910(gBtlLoadHandle), 1);
            func_0024D390(BtlObj_Get(gBtlLoadObj), 0, 2);
            break;
        case BTL_JOB_KIND_OBJECT:
            gBtlLoadObj = func_00249AB8(job->costume, func_0024B910(gBtlLoadHandle), 1);
            func_0020B3C0(job->side, gBtlLoadHandle, gBtlLoadObj);
            break;
        }
        Battle_GetWork()->flags &= ~BATTLE_FLAG_UNK1000;
        BtlJob_Free(job);
        return 1;
    default:
        return 1;
    }
    return 0;
}

/* Job: loads a side's character (model, voice bank, data block) and swaps it in. */
s32 BtlLoad_StepChara(BtlJob *job) {
    BattleRes *res = BATTLE_RES;
    s32 model;
    s32 voice;
    s32 file8;
    s32 file9;
    s32 member;
    s32 ready;
    BtlMemberView *m;

    switch (job->state) {
    case 0:
        if (job->variant != 0) {
            model = job->chara * 10 + job->costume + (BTL_FILE_CHARA + 4);
        } else {
            model = job->chara * 10 + job->costume + BTL_FILE_CHARA;
        }
        if (job->modelOnly == 0) {
            file8 = job->animChara * 10 + (BTL_FILE_CHARA + 8);
            file9 = job->unk1C * 10 + (BTL_FILE_CHARA + 9);
            voice = job->voiceChara + ((gSaveData->flags & SAVE_FLAG_ALT_VOICE) ? BTL_FILE_VOICE_ALT : BTL_FILE_VOICE);
            func_0024B9A8(BattleSide_GetModelSlot(job->side), model, file8, file9);
            res->bank = File_Request3(voice, res->bank, res->bankSize);
            if (!func_0020BEC8(job->side)) {
                BtlMemberView *next;

                member = 0;
                if (job->initial == 0) {
                    member = func_0020B5E8(job->side);
                }
                next = BattleSide_GetMember(job->side, member);
                next->buf[1] = File_Request3(job->chara * 2 + job->side + BTL_FILE_CHARA_DATA, next->buf[1], BTL_MEMBER_BUF_SIZE);
            }
        } else {
            func_0024B9A8(BattleSide_GetModelSlot(job->side), model, -1, -1);
        }
        Battle_GetWork()->flags |= BATTLE_FLAG_UNK800;
        job->state++;
        break;
    case 1:
        if (!File_UpdateRequests()) {
            return 0;
        }
        switch (job->kind) {
        case 0:
            break;
        case BTL_JOB_KIND_CHANGE:
            func_0020B350();
            break;
        }
        job->state++;
        break;
    case 2:
        ready = 1;
        switch (job->kind) {
        case 0:
            break;
        case BTL_JOB_KIND_CHANGE:
            ready = func_0020B368();
            break;
        }
        if (!ready) {
            break;
        }
        job->state++;
        func_0024BAC0();
        if (job->modelOnly == 0) {
            if (job->side == 0) {
                Snd_ReloadBank(0x10, res->bank, 0);
            } else {
                Snd_ReloadBank(0x20, res->bank, 0);
            }
        }
        func_00249BD8(BattleSide_GetObjId(job->side), BattleSide_GetModelSlot(job->side));
        switch (job->kind) {
        case 0:
            func_0024D330(BtlObj_Get(BattleSide_GetObjId(job->side)), 0, 2);
            break;
        case BTL_JOB_KIND_CHANGE:
            func_001C29A0(job->side);
            break;
        }
        if (job->modelOnly == 0) {
            func_001BB1F0(job->side);
            BtlScene_CreateChar(job->side);
            if (!func_0020BEC8(job->side)) {
                m = BattleSide_GetMember(job->side, func_0020B5E8(job->side));
                if (m != NULL) {
                    Res_RelocateOffsets(&m->buf[1], m->buf[1], m->buf[1]);
                    m->data = m->buf[1];
                }
            }
        }
        BtlJob_Free(job);
        Battle_GetWork()->flags &= ~BATTLE_FLAG_UNK800;
        return 1;
    default:
        return 1;
    }
    return 0;
}

/* Job: the first load of a battle. */
s32 BtlLoad_StepInitial(BtlJob *job) {
    BattleRes *res = BATTLE_RES;
    BtlMemberView *m;
    BtlEventView *ev;
    s32 side;
    s32 i;
    s32 id;

    switch (job->state) {
    case 0:
        res->sndCommon = File_Request3(BTL_FILE_SND_COMMON, NULL, 0);
        res->sndStage = File_Request3(Battle_GetStartStage() + BTL_FILE_SND_STAGE, NULL, 0);
        res->sndChara[0] = File_Request3(BattleSide_GetStartChara(0) + ((gSaveData->flags & SAVE_FLAG_ALT_VOICE) ? BTL_FILE_VOICE_ALT : BTL_FILE_VOICE), NULL, 0);
        res->sndChara[1] = File_Request3(BattleSide_GetStartChara(1) + ((gSaveData->flags & SAVE_FLAG_ALT_VOICE) ? BTL_FILE_VOICE_ALT : BTL_FILE_VOICE), NULL, 0);
        job->state++;
        return 0;
    case 1:
        if (!File_UpdateRequests()) {
            return 0;
        }
        job->state++;
        /* fall through */
    case 2:
        Snd_LoadBank(4, res->sndCommon, 0);
        Snd_LoadBank(8, res->sndStage, 0);
        Snd_LoadBank(0x10, res->sndChara[0], 0);
        Snd_LoadBank(0x20, res->sndChara[1], 0);
        job->state++;
        break;
    case 3:
        if (!Snd_IsUploadDone()) {
            break;
        }
        Heap_Free(res->sndCommon);
        res->sndCommon = NULL;
        Heap_Free(res->sndStage);
        res->sndStage = NULL;
        Heap_Free(res->sndChara[0]);
        res->sndChara[0] = NULL;
        Heap_Free(res->sndChara[1]);
        res->sndChara[1] = NULL;
        job->state++;
        break;
    case 4:
        BattleSide_SetModelSlot(0, func_00249C60(0, BattleSide_GetStartChara(0), BattleSide_GetStartCostume(0), BattleSide_GetStartVariant(0)));
        BattleSide_SetModelSlot(1, func_00249C60(1, BattleSide_GetStartChara(1), BattleSide_GetStartCostume(1), BattleSide_GetStartVariant(1)));
        res->stageSize = BTL_STAGE_BUF_SIZE;
        res->stage = Heap_Alloc(res->stageSize, 0x40, 0, HEAP_ANY);
        memset(res->stage, 0, res->stageSize);
        res->bankSize = BTL_BANK_BUF_SIZE;
        res->bank = Heap_Alloc(res->bankSize, 0x40, 0, HEAP_ANY);
        memset(res->bank, 0, res->bankSize);
        for (side = 0; side < 2; side++) {
            for (i = 0; i < 5; i++) {
                m = BattleSide_GetMember(side, i);
                m->buf[0] = Heap_Alloc(BTL_MEMBER_BUF_SIZE, 0x40, 0, HEAP_ANY);
                m->buf[1] = Heap_Alloc(BTL_MEMBER_BUF_SIZE, 0x40, 0, HEAP_ANY);
                m->data = m->buf[0];
                memset(m->buf[0], 0, BTL_MEMBER_BUF_SIZE);
                memset(m->buf[1], 0, BTL_MEMBER_BUF_SIZE);
            }
        }
        if (Battle_IsSplitScreen()) {
            id = Battle_GetStartStage() + BTL_FILE_STAGE_SPLIT;
        } else {
            id = Battle_GetStartStage() + BTL_FILE_STAGE;
        }
        res->stage = File_Request3(id, res->stage, res->stageSize);
        for (side = 0; side < 2; side++) {
            for (i = 0; (u32)i < BattleSide_GetMemberCount(side); i++) {
                m = BattleSide_GetMember(side, i);
                m->buf[0] = File_Request3(m->chara * 2 + side + BTL_FILE_CHARA_DATA, m->buf[0], BTL_MEMBER_BUF_SIZE);
            }
        }
        res->unk8 = File_Request3(gProgress->unk0[0] + 6, NULL, 0);
        job->state++;
        break;
    case 5:
        if (!File_UpdateRequests()) {
            return 0;
        }
        for (side = 0; side < 2; side++) {
            for (i = 0; (u32)i < BattleSide_GetMemberCount(side); i++) {
                m = BattleSide_GetMember(side, i);
                Res_RelocateOffsets(&m->buf[0], m->buf[0], m->buf[0]);
            }
        }
        BtlJob_Free(job);
        return 1;
    case 0x5A:
        res->script = File_Request3(job->kind + BTL_FILE_SCRIPT, NULL, 0);
        job->state++;
        return 0;
    case 0x5B:
        if (!File_UpdateRequests()) {
            return 0;
        }
        ev = Battle_GetEventWork();
        ev->script = func_00257DA0(res->script);
        func_00258050(ev->script, 1000);
        func_00258DB8(ev->script);
        func_00258DB0();
        BattleSetup_Finish();
        job->state = 0;
        break;
    default:
        return 1;
    }
    return 0;
}

/* Starts a plain stage reload unless a load is running or the stage is already loaded. */
s32 BtlLoad_RequestStageReload(s32 stage) {
    BtlJob *job;

    if ((Battle_GetWork()->flags & BATTLE_FLAG_UNK800) || (Battle_GetWork()->flags & BATTLE_FLAG_UNK1000) ||
        (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) || Battle_GetStage() == stage) {
        return 0;
    }
    job = BtlJob_Alloc();
    memset(job, 0, sizeof(BtlJob));
    job->step = BtlLoad_StepStageReload;
    job->state = 0;
    job->kind = stage;
    Battle_SetStage(stage);
    Job_Push((Job *)job);
    return 1;
}

/* Starts an in-battle stage change (with fade) under the same conditions. */
s32 BtlLoad_RequestStageChange(s32 stage) {
    BtlJob *job;

    if ((Battle_GetWork()->flags & BATTLE_FLAG_UNK800) || (Battle_GetWork()->flags & BATTLE_FLAG_UNK1000)) {
        return 0;
    }
    if ((Battle_GetWork()->flags & BATTLE_FLAG_LOADING) || (stage >= 0 && Battle_GetStage() == stage)) {
        return 0;
    }
    if (stage < 0) {
        return 0;
    }
    job = BtlJob_Alloc();
    memset(job, 0, sizeof(BtlJob));
    job->step = BtlLoad_StepStageChange;
    job->state = 0;
    job->kind = stage;
    Battle_SetStage(stage);
    Job_Push((Job *)job);
    Fade_Start(2, 1, 1.0f);
    return 1;
}

/* Per frame: turns a pending type-1 request of the fighter manager into a BtlLoad_StepObject job. */
void BtlLoad_PollObjectRequest(void) {
    s32 id;
    s32 costume;
    s32 variant;
    s32 slot;
    s32 side;
    BtlJob *job;

    for (side = 0; side < 2; side++) {
        if (func_0020B248(side)) {
            func_0020B290(&id, &costume, &variant, NULL, NULL, NULL, &slot);
            job = BtlJob_Alloc();
            memset(job, 0, sizeof(BtlJob));
            job->step = BtlLoad_StepObject;
            job->side = side;
            job->kind = BTL_JOB_KIND_OBJECT;
            job->costume = slot;
            job->chara = id;
            job->animChara = costume;
            job->unk1C = variant;
            Job_Push((Job *)job);
            func_0020B338();
        }
    }
}

/* Per frame: turns a pending type-0 request (character change) into a BtlLoad_StepChara job. */
void BtlLoad_PollCharaRequest(void) {
    s32 chara;
    s32 costume;
    s32 variant;
    s32 animChara;
    s32 unk1C;
    s32 voiceChara;
    s32 side;
    s32 modelOnly;
    BtlJob *job;

    for (side = 0; side < 2; side++) {
        if (func_0020B200(side)) {
            func_0020B290(&chara, &costume, &variant, &animChara, &unk1C, &voiceChara, NULL);
            if (animChara < 0 && unk1C < 0 && voiceChara < 0) {
                modelOnly = 1;
            } else {
                modelOnly = 0;
            }
            job = BtlJob_Alloc();
            memset(job, 0, sizeof(BtlJob));
            job->side = side;
            job->kind = BTL_JOB_KIND_CHANGE;
            job->step = BtlLoad_StepChara;
            job->chara = chara;
            job->animChara = animChara;
            job->unk1C = unk1C;
            job->voiceChara = voiceChara;
            job->costume = costume;
            job->variant = variant;
            job->modelOnly = modelOnly;
            job->initial = 0;
            if (modelOnly == 0) {
                BtlScene_FreeChar(side);
            }
            BattleSide_SetForm(job->side, job->chara, job->costume, job->variant);
            Job_Push((Job *)job);
            func_0020B338();
        }
    }
}

/* Restart: drops running jobs and reloads the initial stage / characters where they changed (blocking). */
void BtlLoad_Reload(void) {
    BattleRes *res = BATTLE_RES;
    s32 side;
    s32 i;
    s32 chara;
    s32 costume;
    s32 variant;
    BtlJob *job;
    BtlMemberView *m;

    if (res != NULL && res->transition != NULL) {
        Battle_GetWork()->flags &= ~BATTLE_FLAG_LOADING;
        Heap_Free(res->transition);
        res->transition = NULL;
    }
    func_00249CD8();
    Job_Clear();
    BtlJob_InitPool();
    /* The original passes a second argument (0) that the function does not have. */
    ((s32 (*)(s32, s32))BtlLoad_RequestStageReload)(Battle_GetStartStage(), 0);
    for (side = 0; side < 2; side++) {
        if (BattleSide_IsFormChanged(side)) {
            chara = BattleSide_GetStartChara(side);
            costume = BattleSide_GetStartCostume(side);
            variant = BattleSide_GetStartVariant(side);
            BattleSide_SetForm(side, chara, costume, variant);
            job = BtlJob_Alloc();
            memset(job, 0, sizeof(BtlJob));
            job->step = BtlLoad_StepChara;
            job->side = side;
            job->kind = BTL_JOB_KIND_RESTART;
            job->chara = chara;
            job->animChara = chara;
            job->unk1C = chara;
            job->voiceChara = chara;
            job->costume = costume;
            job->variant = variant;
            job->modelOnly = 0;
            job->initial = 1;
            BtlScene_FreeChar(side);
            Job_Push((Job *)job);
        } else {
            func_00249BD8(BattleSide_GetObjId(side), BattleSide_GetModelSlot(side));
        }
    }
    if (BtlJob_GetFreeCount() != BTL_JOB_COUNT) {
        Load_RunBlocking();
    }
    for (side = 0; side < 2; side++) {
        for (i = 0; (u32)i < BattleSide_GetMemberCount(side); i++) {
            m = BattleSide_GetMember(side, i);
            m->data = m->buf[0];
        }
    }
}

/* Resets the job pool and queues the first load. */
void BtlLoad_PushInitialJob(void) {
    BtlJob *job;

    BtlJob_InitPool();
    job = BtlJob_Alloc();
    memset(job, 0, sizeof(BtlJob));
    job->step = BtlLoad_StepInitial;
    job->kind = Battle_GetScript();
    if (job->kind >= 0) {
        job->state = 0x5A;
    }
    Job_Push((Job *)job);
}

/* Frees every buffer of the first load and unloads the four battle sound banks. */
void BtlLoad_FreeAll(void) {
    BattleRes *res = BATTLE_RES;
    BtlEventView *ev = Battle_GetEventWork();
    BtlMemberView *m;
    s32 side;
    s32 i;

    if (Battle_GetMode() == 1) {
        func_00257E80(ev->script);
        Heap_Free(res->script);
        res->script = NULL;
    }
    for (side = 0; side < 2; side++) {
        for (i = 0; i < 5; i++) {
            m = BattleSide_GetMember(side, i);
            Heap_Free(m->buf[0]);
            m->buf[0] = NULL;
            Heap_Free(m->buf[1]);
            m->buf[1] = NULL;
        }
    }
    Heap_Free(res->bank);
    res->bank = NULL;
    Heap_Free(res->unk8);
    res->unk8 = NULL;
    Heap_Free(res->stage);
    res->stage = NULL;
    if (res != NULL && res->transition != NULL) {
        Battle_GetWork()->flags &= ~BATTLE_FLAG_LOADING;
        Heap_Free(res->transition);
        res->transition = NULL;
    }
    Snd_Reset();
    Snd_UnloadBank(4);
    Snd_UnloadBank(8);
    Snd_UnloadBank(0x10);
    Snd_UnloadBank(0x20);
}

/* Zeroes the result block. */
void BattleResult_Clear(void) {
    memset(Battle_GetResult(), 0, sizeof(BattleResult));
}

/* Stores the winner bits and the reason bits; a restart reason is forwarded to BattleReplay_SetActive. */
void BattleResult_Set(s32 flags, s32 reason) {
    BattleResult *result = Battle_GetResult();

    result->flags = flags;
    result->reason = reason;
    if (reason & 0x8000) {
        BattleReplay_SetActive(0);
    }
    if (reason & 0x10000) {
        BattleReplay_SetActive(1);
    }
}

/* The battle was left without a finish (winner bit 3). */
s32 BattleResult_IsAborted(void) {
    if (Battle_GetResult()->flags & 8) {
        return 1;
    }
    return 0;
}

/* A restart was asked for (reason bits 15 / 16). */
s32 Battle_IsRematchRequested(void) {
    return (Battle_GetResult()->reason & 0x18000) != 0;
}

/* One of the sides won. */
s32 BattleResult_HasWinner(void) {
    return (Battle_GetResult()->flags & 3) != 0;
}

/* Side 0 won; always 1 in split-screen and in modes 8 and 1. */
s32 BattleResult_IsPlayerWin(void) {
    BattleResult *result = Battle_GetResult();

    if (Battle_IsSplitScreen() || Battle_GetMode() == 8 || Battle_GetMode() == 1) {
        return 1;
    }
    if (result->flags & 1) {
        return 1;
    }
    return 0;
}

/* Reason bit 20. */
s32 BattleResult_IsReasonBit20(void) {
    return (Battle_GetResult()->reason >> 20) & 1;
}

/* Index of the winning side: 1 only when side 1 won and side 0 did not. */
s32 BattleResult_GetWinnerSide(void) {
    s32 flags = Battle_GetResult()->flags;

    if (flags & 1) {
        return 0;
    }
    if (flags & 2) {
        return 1;
    }
    return 0;
}

/* Reason bit 0: K.O. */
s32 BattleResult_IsKo(void) {
    return Battle_GetResult()->reason & 1;
}

/* Reason bit 1: time up. */
s32 BattleResult_IsTimeUp(void) {
    return (Battle_GetResult()->reason >> 1) & 1;
}

/* Reason bit 2. */
s32 BattleResult_IsReasonBit2(void) {
    return (Battle_GetResult()->reason >> 2) & 1;
}

/* Reason bit 18. */
s32 BattleResult_IsReasonBit18(void) {
    return (Battle_GetResult()->reason >> 18) & 1;
}

/* Reason bits 5 or 6. */
s32 BattleResult_IsReasonBit5or6(void) {
    return (Battle_GetResult()->reason & 0x60) != 0;
}

/* Event 0x59 is clear in the winning side's event set. */
s32 BattleResult_IsWinnerEvent59Clear(void) {
    return BtlEvent_WasRaised(BattleResult_GetWinnerSide(), 0x59) == 0;
}

/* Event 0x3C is set in the given event set. */
s32 BattleResult_IsEvent3CSet(s32 side) {
    return BtlEvent_WasRaised(side, 0x3C) != 0;
}

#define RESULT_EVENT(ev, bit) \
    if (BtlEvent_WasRaised(0, ev)) { \
        result->unk10 |= (bit); \
    }

/* Rebuilds the result's 64-bit event summary from event set 0. */
void BattleResult_CollectEvents(BattleResult *result) {
    u32 i;
    u32 count;
    u16 sum;
    s32 j;
    BtlMemberView *m;

    result->unk10 = 0;
    RESULT_EVENT(0x51, 1);
    RESULT_EVENT(0x52, 2);
    RESULT_EVENT(0x53, 4);
    if (!BtlEvent_WasRaised(0, 0x59)) {
        result->unk10 |= 8;
    }
    RESULT_EVENT(0x54, 0x10);
    if (BtlEvent_WasRaised(0, 0x37) && BtlEvent_WasRaised(0, 0x38) && BtlEvent_WasRaised(0, 0x39) && BtlEvent_WasRaised(0, 0x3A) &&
        BtlEvent_WasRaised(0, 0x3B)) {
        result->unk10 |= 0x100;
    }
    RESULT_EVENT(0x55, 0x200);
    RESULT_EVENT(0x57, 0x400);
    RESULT_EVENT(0x34, 0x2000);
    RESULT_EVENT(0x3E, 0x8000);
    RESULT_EVENT(0x32, 0x10000);
    RESULT_EVENT(0x3F, 0x20000);
    RESULT_EVENT(0x40, 0x40000);
    RESULT_EVENT(0x56, 0x80000);
    RESULT_EVENT(0x36, 0x1000000);
    RESULT_EVENT(0x24, 0x2000000);
    RESULT_EVENT(0x25, 0x2000000);
    RESULT_EVENT(0x26, 0x4000000);
    RESULT_EVENT(0x27, 0x4000000);
    RESULT_EVENT(0x28, 0x8000000);
    RESULT_EVENT(0x1F, 0x10000000);
    RESULT_EVENT(0x33, 1UL << 31);
    RESULT_EVENT(0x41, 1UL << 32);
    RESULT_EVENT(0x45, 1UL << 33);
    RESULT_EVENT(0x42, 1UL << 34);
    RESULT_EVENT(0x43, 1UL << 35);
    RESULT_EVENT(0x44, 1UL << 36);
    if (!BtlEvent_WasRaised(0, 0x22)) {
        result->unk10 |= 1UL << 37;
    }
    RESULT_EVENT(0x58, 1UL << 39);
    if (!BtlEvent_WasRaised(0, 0x4E) && BattleSide_GetMemberCount(0) >= 2) {
        result->unk10 |= 1UL << 40;
    }
    RESULT_EVENT(0x21, 1UL << 41);
    RESULT_EVENT(0x3C, 1UL << 42);
    RESULT_EVENT(0x1E, 1UL << 44);
    count = BattleSide_GetMemberCount(0);
    sum = 0;
    for (i = 0; i < count; i++) {
        m = BattleSide_GetMember(0, i);
        if (m != NULL) {
            for (j = 0; j < 8; j++) {
                sum += m->unk14[j];
            }
        }
    }
    if (sum == 0) {
        result->unk10 |= 1UL << 45;
    }
    RESULT_EVENT(0x46, 1UL << 46);
    RESULT_EVENT(0x47, 1UL << 47);
}
