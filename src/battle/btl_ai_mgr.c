#include "common.h"
#include "battle/btl_ai_mgr.h"
#include "battle/battle.h"
#include "battle/btl_seq.h"
#include "sys/heap.h"
#include "sys/rand.h"

/*
 * CPU opponent manager and the "move" action: 0x1BB128..0x1BC8A8.
 *
 * gBtlAi is one heap block holding the AI data pointer, three values shared by both sides (distance between
 * the fighters, their radius sum, the line-of-sight test) and one 0x520-byte controller per side.
 *
 * BtlAiMgr_Update is the frame entry (Battle_Loop, after Battle_UpdateWork, before the fighters sample
 * input). It does nothing while the battle is paused (flag 0x100), an object load job runs (0x1000), the
 * stage is not ready, or the sequence is not in state 2 (Ready) or 3 (Fight). Otherwise it refreshes the
 * shared values, counts the frame, and for each of the first two fighters that takes AI input
 * (fighter +0x1278) and has parameters, runs four passes that live outside this file: func_001BFF70,
 * func_001BAC30, BtlAi_RunSeq, BtlAi_SendInput.
 *
 * The second half of the file (0x1BB730..) is one "action": a table of four phase functions
 * (gBtlAiMovePhases = {Init, Start, Run, End}) indexed by act.phase and called by the dispatcher
 * func_001BC8A8. It moves the fighter to a target computed from the opponent's position: straight when the
 * stage does not block the way, otherwise along a path from func_001B3A50. Its only output is the side's
 * virtual pad, through func_001BC918 / func_001BCB78 (AI button bits: BTLAI_BTN_* in the header).
 *
 * Randomness: Rand_Range (the shared MT19937) once in BtlAiMove_PickDir (once per start of the action) and
 * twice in BtlAiMove_CalcTarget when the move type is 4. Outside the simulation state this file reads only
 * battle flags 0x100 / 0x1000, BtlStage_IsReady and the sequence state, all in BtlAiMgr_Update. No pad,
 * sound or file state.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern void Vec3_Add(BtlAiVec *dst, BtlAiVec *a, BtlAiVec *b);
extern void Vec3_Sub(BtlAiVec *dst, BtlAiVec *a, BtlAiVec *b);
extern void Vec4_Scale(BtlAiVec *dst, BtlAiVec *src, f32 k);
extern void Vec3_Normalize(BtlAiVec *dst, BtlAiVec *src);
extern f32 Vec3_Length(BtlAiVec *v);
extern f32 Vec3_Dot(BtlAiVec *a, BtlAiVec *b);
extern void func_00121E20(BtlAiVec *v);                         /* v = (0, 0, 0, 0) */
extern void func_00122868(BtlAiVec *dst, BtlAiVec *src, f32 angle); /* rotate about Y */
extern f32 Mathf_Cos(f32 angle);                        /* cos */
extern f32 Mathf_Sin(f32 angle);                        /* sin */

extern s32 BtlStage_IsReady(void);
extern s32 BtlChar_GetCount(void);
extern BtlAiMgrChr *BtlChar_Get(s32 side);
extern s32 BtlChar_IsStage4Or27(void);                        /* Battle_GetStage() is 4 or 27 */

/* Fighter accessors by object id (0x204xxx..0x208xxx, not decompiled). */
extern void func_002053F0(s32 objId, BtlAiVec *out);       /* position: transform +0 plus +0x20, or object +0x970 */
extern f32 func_00204EA0(s32 objId);                   /* object +0xFF4 (height), 10.0 without object */
extern f32 func_00205870(s32 objId);                   /* fighter transform +0xB4 */
extern f32 func_00205800(s32 objId);                   /* fighter transform +0x98 */
extern f32 func_002062F0(s32 objId);                   /* body radius: object +0x91C->+0xC, or +0xFF0 * constant */
extern void func_002056F8(s32 objId, BtlAiVec *out);       /* fighter transform +0x30 */
extern s32 func_00205CC8(s32 objId);                   /* BtlChar_TestFlag(chr, 0x13) */
extern s32 BtlCharApi_TestFlag0F(s32 objId);                   /* BtlChar_TestFlag(chr, 0xF) */
extern s32 BtlCharApi_GetUnk974(s32 objId);                   /* fighter +0x974: current state id */
extern s32 BtlCharApi_IsInputInjected(s32 objId);                   /* fighter +0x1278: input comes from the AI */

/* Stage (0x23Fxxx / 0x242xxx, not decompiled). */
extern f32 func_0023FE70(void);                        /* stage limit: first float of the stage's +0x40 block */
extern f32 func_0023FEB0(void);                        /* the same - 100 */
extern f32 func_0023FEF8(void);                        /* second float of that block */
extern void func_002427A0(s32 objId, BtlAiVec *out, BtlAiVec *out2, s32 arg);

/* Other AI code. */
extern s32 func_001B2DF0(BtlAiSegment *seg);           /* stage line test; fills the BtlAiHit block */
extern BtlAiHit *func_001B2F40(void);
extern void func_001B3A50(BtlAiVec *from, BtlAiVec *to, BtlAiMovePath *path);
extern void func_001B3F78(BtlAiMgrAct *act);           /* clears the action state */
extern void BtlAi_RunSeq(BtlAiMgrSide *s);             /* 0x1B6BA0 (btl_ai.c) */
extern void BtlAi_SendInput(BtlAiMgrSide *s);          /* 0x1B6CD8 (btl_ai.c) */
extern s32 BtlAi_GetPairRate(BtlAiMgrSide *s, s32 button, s32 arg, void *tblA, void *tblB, s32 arg5);
extern s32 func_001B82C0(s32 button, s32 arg);
extern void func_001BA308(BtlAiMgrSide *s);
extern void func_001BAC30(BtlAiMgrSide *s);
extern void func_001BAD50(s32 arg);                    /* binds the AI data section of gCommonRes */
extern void func_001BAF68(s32 side, s32 arg);          /* resets one side's controller from its fighter */
extern void func_001BC918(BtlAiMgrOutput *out, s32 keep); /* clears the virtual pad */
/* Writes the virtual pad. hold / press / once are AI button bits (BTLAI_BTN_*): hold is passed on every
   frame, press only for a button not marked 1 in the output's per-button word, once only for a button not
   yet marked 2 (and marks it). x, y are averaged with the previous stick. The parameter order is the one
   that makes every call here match. */
extern void func_001BCB78(BtlAiMgrSide *s, s32 hold, s32 press, s32 once, s16 special, f32 x, f32 y);

/* The divisions by 10 in BtlAiMove_Start are real divide instructions with 10 in a register, reloaded for
   each one: that is what a constant passed to an inlined helper gives (a literal gives a multiply). */
static inline s32 BtlAiMove_Div(s32 a, s32 b) {
    return a / b;
}

static inline s32 BtlAiMove_Mod(s32 a, s32 b) {
    return a % b;
}
extern void func_001BFF70(BtlAiMgrSide *s);

extern BtlAiMgr *gBtlAi;

/* Frees what the block owns, rebinds the shared AI data and resets both sides from their fighters. */
void BtlAiMgr_Reset(void) {
    BtlAiMgrSide *s;
    s32 i;

    if (gBtlAi->flags & 1) {
        if (gBtlAi->data != NULL) {
            Heap_Free(gBtlAi->data);
            gBtlAi->data = NULL;
        }
    }
    for (s = gBtlAi->side; s != &gBtlAi->side[2]; s++) {
        if (s->flags & 1) {
            if (s->param != NULL) {
                Heap_Free(s->param);
                s->param = NULL;
            }
        }
    }
    func_001BAD50(0);
    for (i = 0; i < 2; i++) {
        func_001BAF68(i, 0);
    }
}

/* Resets one side's controller from its fighter. */
void BtlAiMgr_ResetSide(s32 side) {
    func_001BAF68(side, 0);
}

/* Allocates the manager block if needed and resets it. */
void BtlAiMgr_Init(void) {
    if (gBtlAi == NULL) {
        memset(gBtlAi = Heap_Alloc(0xA60, 0x20, 0, 2), 0, 0xA60);
    }
    BtlAiMgr_Reset();
}

/* Frees what the block owns, then the block. */
void BtlAiMgr_Term(void) {
    BtlAiMgrSide *s;

    if (gBtlAi->flags & 1) {
        if (gBtlAi->data != NULL) {
            Heap_Free(gBtlAi->data);
            gBtlAi->data = NULL;
        }
    }
    for (s = gBtlAi->side; s != &gBtlAi->side[2]; s++) {
        if (s->flags & 1) {
            if (s->param != NULL) {
                Heap_Free(s->param);
                s->param = NULL;
            }
        }
    }
    if (gBtlAi != NULL) {
        Heap_Free(gBtlAi);
        gBtlAi = NULL;
    }
}

/* Fills the side's 14 per-button rates from its aiType table. */
void BtlAiMgr_BuildRates(BtlAiMgrSide *s) {
    BtlAiMgrTypeTbl *tbl = gBtlAi->data->typeTbl[s->aiType];
    s32 i;

    for (i = 0; i < 14; i++) {
        s32 r;

        s->rate[i] = 0;
        r = func_001B82C0(i, 3);
        if (r != -1) {
            s->rate[i] = BtlAi_GetPairRate(s, i, r, tbl->unk004, tbl->unk2C4, 0);
        }
    }
}

/* Returns a side's aiType. */
s32 BtlAiMgr_GetType(s32 side) {
    return gBtlAi->side[side].aiType;
}

/* Sets a side's aiType, rebuilds what depends on it and clears the side's work and action state. */
void BtlAiMgr_SetType(s32 side, s32 aiType) {
    BtlAiMgrSide *s = &gBtlAi->side[side];

    s->aiType = aiType;
    BtlAiMgr_BuildRates(s);
    func_001BA308(s);
    memset(s->work, 0, 0x188);
    func_001B3F78(&s->act);
}

/* Returns a side's CPU level. */
s32 BtlAiMgr_GetLevel(s32 side) {
    return gBtlAi->side[side].cpuLevel;
}

/* Sets a side's CPU level, rebuilds what depends on it and clears the side's work and action state. */
void BtlAiMgr_SetLevel(s32 side, s32 cpuLevel) {
    BtlAiMgrSide *s = &gBtlAi->side[side];

    s->cpuLevel = cpuLevel;
    BtlAiMgr_BuildRates(s);
    func_001BA308(s);
    memset(s->work, 0, 0x188);
    func_001B3F78(&s->act);
}

/* Returns 1 when the battle sequence is in neither state 2 (Ready) nor 3 (Fight): the AI does not run. */
s32 BtlAiMgr_IsIdleState(void) {
    s32 state = BtlSeq_GetState();

    if (state == 2 || state == 3) {
        return 0;
    }
    return 1;
}

/* Measures the distance between the two fighters' centres and tests whether the stage blocks the line. */
void BtlAiMgr_UpdateSight(void) {
    BtlAiVec a;
    BtlAiVec b;
    BtlAiVec d;
    BtlAiSegment seg;

    func_002053F0(0, &a);
    a.y -= func_00204EA0(0) * 0.5f;
    func_002053F0(1, &b);
    b.y -= func_00204EA0(1) * 0.5f;
    Vec3_Sub(&d, &a, &b);
    gBtlAi->dist = Vec3_Length(&d);
    seg.from = a;
    seg.to = b;
    gBtlAi->sight = 0;
    if (func_001B2DF0(&seg)) {
        gBtlAi->sight |= BTLAI_SIGHT_BLOCKED;
        if (func_001B2F40()->id != -1) {
            gBtlAi->sight |= BTLAI_SIGHT_BLOCKED_ID;
        }
    }
}

/* Returns the number of frames the AI has run. */
s32 BtlAiMgr_GetFrame(void) {
    return gBtlAi->frame;
}

/* Frame entry: runs both sides' controllers while the fight is live. */
void BtlAiMgr_Update(void) {
    s32 i;

    if (gBtlAi == NULL) {
        return;
    }
    if (Battle_GetWork()->flags & 0x100) {
        return;
    }
    if (Battle_GetWork()->flags & 0x1000) {
        return;
    }
    if (!BtlStage_IsReady()) {
        return;
    }
    if (BtlAiMgr_IsIdleState()) {
        return;
    }
    BtlAiMgr_UpdateSight();
    gBtlAi->frame++;
    for (i = 0; i < BtlChar_GetCount() && i < 2; i++) {
        BtlAiMgrSide *s = &gBtlAi->side[i];

        if (BtlCharApi_IsInputInjected(i) && s->param != NULL) {
            func_001BFF70(s);
            func_001BAC30(s);
            BtlAi_RunSeq(s);
            BtlAi_SendInput(s);
        }
    }
}

/* Picks at random one of the directions the stage limits allow: 0 always; in mode 2 also 1 when a fighter
   is above the upper limit and 2 when one is below its own limit; 3 / 4 for the two sides, dropped when
   the fighter is near the stage edge and that side leads outwards. */
s32 BtlAiMove_PickDir(BtlAiMgrSide *s, s32 mode) {
    BtlAiVec pos;
    BtlAiVec opp;
    BtlAiVec d;
    BtlAiVec out;
    BtlAiVec r;
    struct {
        s32 v[8];
    } cand;
    typeof(cand) *list;
    s32 n = 1;
    f32 top = func_0023FEF8() + 20.0f;
    f32 rad = func_0023FEB0() - 100.0f;

    func_002053F0(s->side, &pos);
    func_002053F0(s->side ^ 1, &opp);
    list = &cand;
    list->v[0] = 0;
    if (mode == 2) {
        if (top < pos.y || top < opp.y) {
            cand.v[1] = 1;
            n = 2;
        }
        if (pos.y < func_00205870(s->side) - 20.0f || opp.y < func_00205870(s->side ^ 1) - 20.0f) {
            list->v[n++] = 2;
        }
    }
    if (rad < Vec3_Length(&pos)) {
        Vec3_Sub(&d, &opp, &pos);
        d.y = 0.0f;
        Vec3_Normalize(&d, &d);
        Vec3_Normalize(&out, &pos);
        if (0.0f < Vec3_Length(&d) && 0.0f < Vec3_Length(&out)) {
            if (0.0f < Vec3_Dot(&out, &d)) {
                goto both;
            }
            func_00122868(&r, &d, -1.0471975f);
            Vec3_Normalize(&r, &r);
            if (0.0f < Vec3_Dot(&out, &r)) {
                list->v[n++] = 3;
            }
            func_00122868(&r, &d, 1.0471975f);
            Vec3_Normalize(&r, &r);
            if (0.0f < Vec3_Dot(&out, &r)) {
                list->v[n++] = 4;
            }
        }
    } else {
    both:
        list->v[n++] = 3;
        list->v[n++] = 4;
    }
    return list->v[Rand_Range(n)];
}

/* Computes the move target from the opponent's position and the move type, clamps it to the stage limit,
   and sets the three "blocked" flags from stage line tests. */
void BtlAiMove_CalcTarget(BtlAiMgrSide *s) {
    BtlAiVec pos;
    BtlAiVec opp;
    BtlAiVec a;
    BtlAiVec dir;
    BtlAiVec ahead;
    BtlAiVec tmp;
    BtlAiSegment seg;
    BtlAiMgrAct *act = &s->act;
    BtlAiMoveWork *m = &s->move;
    f32 rad = func_0023FE70();
    s32 special = BtlChar_IsStage4Or27();
    f32 reach;

    func_002053F0(s->side, &pos);
    a = pos;
    func_002053F0(s->side ^ 1, &opp);
    a.y = pos.y;
    Vec3_Sub(&dir, &a, &opp);
    Vec3_Normalize(&dir, &dir);
    Vec4_Scale(&a, &dir, m->dist[m->type]);
    if (special != 0 && (u32)m->type >= 2) {
        func_00121E20(&m->target);
        m->target.y = pos.y;
    } else if (m->type == 4) {
        func_002427A0(s->side, &m->target, &tmp, 0);
        m->target.y -= 20.0f;
        m->target.x = (s32)(Rand_Range(400) - 200);
        m->target.z = (s32)(Rand_Range(400) - 200);
    } else {
        Vec3_Add(&m->target, &opp, &a);
    }
    pos.y -= func_00204EA0(s->side) * 0.5f;
    act->flags &= ~(BTLAI_ACT_BLOCKED | BTLAI_ACT_BLOCKED_ID);
    if (rad < Vec3_Length(&m->target)) {
        Vec4_Scale(&a, &dir, rad - Vec3_Length(&pos));
        Vec3_Add(&m->target, &pos, &a);
    }
    seg.from = pos;
    seg.to = m->target;
    if (func_001B2DF0(&seg)) {
        act->flags |= BTLAI_ACT_BLOCKED;
        if (func_001B2F40()->id != -1) {
            act->flags |= BTLAI_ACT_BLOCKED_ID;
        }
    }
    reach = func_002062F0(s->side);
    reach += func_00205800(s->side) * 3.0f;
    func_002056F8(s->side, &dir);
    Vec3_Normalize(&dir, &dir);
    Vec4_Scale(&a, &dir, reach);
    Vec3_Add(&ahead, &pos, &a);
    if (rad < Vec3_Length(&ahead)) {
        Vec4_Scale(&a, &dir, rad - Vec3_Length(&pos));
        Vec3_Add(&ahead, &pos, &a);
    }
    act->flags &= ~BTLAI_ACT_BLOCKED_AHEAD;
    ahead.y = pos.y;
    seg.from = pos;
    seg.to = ahead;
    if (func_001B2DF0(&seg)) {
        act->flags |= BTLAI_ACT_BLOCKED_AHEAD;
    }
}

/* Returns 1 when the fighter is within 20 units, or within five times fighter transform +0x98, of a point.
   The height difference counts only for move type 1 with flat == 0. */
s32 BtlAiMove_IsNear(BtlAiMgrSide *s, BtlAiVec *p, s32 flat) {
    BtlAiVec pos;
    BtlAiVec d;
    BtlAiMoveWork *m = &s->move;
    f32 len;

    func_002053F0(s->side, &pos);
    Vec3_Sub(&d, &pos, p);
    if (m->type != 1 || flat != 0) {
        d.y = 0.0f;
    }
    len = Vec3_Length(&d);
    if (len < 20.0f) {
        return 1;
    }
    if (len < func_00205800(s->side) * 5.0f) {
        return 1;
    }
    return 0;
}

/* Builds the path from the fighter to the target. Returns 1 when the path is empty. */
s32 BtlAiMove_BuildPath(BtlAiMgrSide *s) {
    BtlAiVec pos;
    BtlAiVec d;
    BtlAiMovePath *path = &s->move.path;
    BtlAiMoveWork *m = &s->move;

    func_002053F0(s->side, &pos);
    func_001B3A50(&pos, &s->move.target, path);
    m->pathTimer = 60;
    if (path->count > 0) {
        Vec3_Sub(&d, (BtlAiVec *)&path->pts[path->count - 1], &pos);
        m->remain = Vec3_Length(&d);
        return 0;
    }
    m->remain = 0.0f;
    return 1;
}

/* Decides what the run phase does this frame: 0 recompute the target, 1 arrived, 2 head straight for the
   target, 3 follow the path, 4 wait (fighter state 0x1F..0x22). */
s32 BtlAiMove_Check(BtlAiMgrSide *s) {
    BtlAiVec pos;
    BtlAiVec opp;
    BtlAiVec d;
    BtlAiSegment seg;
    s32 skip = 0;
    BtlAiMoveWork *m = &s->move;
    BtlAiMgrAct *act = &s->act;
    BtlAiMovePath *path = &s->move.path;
    s32 state = BtlCharApi_GetUnk974(s->side);

    if (m->pathTimer > 0) {
        m->pathTimer--;
    }
    if (act->flags & BTLAI_ACT_PATH) {
        if (!(act->flags & BTLAI_ACT_BLOCKED)) {
            return 0;
        }
        if (path->count == 0) {
            return 0;
        }
        func_002053F0(s->side, &pos);
        if (path->count >= 2) {
            seg.from = pos;
            *(BtlAiMovePoint *)&seg.to = path->pts[path->count - 2];
            skip = func_001B2DF0(&seg) == 0;
        }
        if (skip || BtlAiMove_IsNear(s, (BtlAiVec *)&path->pts[path->count - 1], 1)) {
            path->count--;
            m->stuckTimer = 0;
            if (path->count == 0) {
                return 1;
            }
            if ((u32)(state - 0x1F) < 4) {
                return 4;
            }
            Vec3_Sub(&d, (BtlAiVec *)&path->pts[path->count - 1], &pos);
            m->remain = Vec3_Length(&d);
        }
        if (m->pathTimer == 0) {
            func_002053F0(s->side ^ 1, &opp);
            Vec3_Sub(&d, &opp, (BtlAiVec *)path);
            if (50.0f < Vec3_Length(&d)) {
                if (BtlAiMove_BuildPath(s)) {
                    return 0;
                }
            }
        }
        return 3;
    }
    if (BtlAiMove_IsNear(s, &m->target, 0)) {
        if (!(gBtlAi->sight & BTLAI_SIGHT_BLOCKED)) {
            return 1;
        }
    }
    if (!(act->flags & BTLAI_ACT_BLOCKED)) {
        return 2;
    }
    if ((act->flags & BTLAI_ACT_BLOCKED_ID) && m->mode == 2) {
        return 2;
    }
    return BtlAiMove_BuildPath(s) ? 0 : 3;
}

/* Writes the virtual pad to move toward a point: stick direction relative to chr->yaw (clamped to -1..1),
   ASCEND / DESCEND from the height difference, DASH held (fighter flags 0x13 and 0xF both clear) or pressed.
   While fighter flag 0x13 is set, CHARGE is added for move types 2..4 and the stick is reduced to its
   dominant axis. Within dist[2] of the point (kind 1, types 0 and 1) the stick is released and UP held. */
void BtlAiMove_Steer(BtlAiMgrSide *s, BtlAiVec *to, s32 kind) {
    BtlAiVec pos;
    BtlAiVec d;
    BtlAiVec opp;
    s32 *volatile flags; /* volatile: the original kept this pointer in a stack slot and stored it between two loads */
    s32 cls;
    s32 hold = 0;
    BtlAiMoveWork *m = &s->move;
    s32 press = 0;
    BtlAiMgrChr *chr = BtlChar_Get(s->side);
    BtlAiMgrTables *tbl;
    s32 side;
    BtlAiMgr *mgr;
    s32 fly;
    f32 dy;
    f32 len;
    f32 yaw;
    f32 x;
    f32 y;

    mgr = gBtlAi;
    side = s->side;
    flags = &s->act.flags;
    tbl = mgr->data->tables;
    cls = tbl->stateClass[BtlCharApi_GetUnk974(side)];
    fly = BtlCharApi_TestFlag0F(s->side);
    func_002053F0(s->side, &pos);
    func_002053F0(s->side ^ 1, &opp);
    dy = to->y - pos.y;
    Vec3_Sub(&d, to, &pos);
    d.y = d.w = 0.0f;
    len = Vec3_Length(&d);
    if (len < func_002062F0(s->side)) {
        len = func_002062F0(s->side);
    }
    Vec3_Normalize(&d, &d);
    yaw = chr->yaw;
    x = Mathf_Cos(yaw) * d.x - Mathf_Sin(yaw) * d.z;
    y = -(Mathf_Sin(yaw) * d.x + Mathf_Cos(yaw) * d.z);
    if (1.0f < x) {
        x = 1.0f;
    } else if (x < -1.0f) {
        x = -1.0f;
    }
    if (1.0f < y) {
        y = 1.0f;
    } else if (y < -1.0f) {
        y = -1.0f;
    }
    if (func_00205CC8(s->side) || fly != 0) {
        press = BTLAI_BTN_DASH;
    } else {
        hold = BTLAI_BTN_DASH;
    }
    if (kind == 0) {
        if (dy < -30.0f) {
            if (fly == 0 || !(pos.y < opp.y)) {
                hold |= BTLAI_BTN_ASCEND;
            }
        } else if (30.0f < dy) {
            hold |= BTLAI_BTN_DESCEND;
        }
    }
    if (*flags & BTLAI_ACT_BLOCKED_AHEAD) {
        if (!(hold & BTLAI_BTN_DESCEND)) {
            hold |= BTLAI_BTN_ASCEND;
        }
    }
    if (cls == 11) {
        press &= ~BTLAI_BTN_DASH;
    }
    if (BtlCharApi_TestFlag0F(s->side) && !BtlCharApi_TestFlag0F(s->side ^ 1)) {
        if (pos.y > opp.y) {
            hold |= BTLAI_BTN_ASCEND;
        }
    }
    if (len < m->dist[2] && kind == 1 && (u32)m->type < 2) {
        y = 0.0f;
        hold |= BTLAI_BTN_UP;
        x = y;
    }
    if (func_00205CC8(s->side)) {
        if (!(hold & BTLAI_BTN_ASCEND)) {
            if ((u32)m->type >= 2) {
                hold |= BTLAI_BTN_CHARGE;
            }
            if (((x < 0.0f) ? -x : x) > ((y < 0.0f) ? -y : y)) {
                y = 0.0f;
            } else {
                x = 0.0f;
            }
        }
    }
    func_001BCB78(s, hold, press, 0, 0, x, y);
}

/* Virtual pad for move type 0 on a clear line: no stick, DASH held (plus CHARGE in mode 2, ASCEND when
   blocked ahead), stick Y -1 in state class 10. */
void BtlAiMove_Hold(BtlAiMgrSide *s) {
    BtlAiMoveWork *m = &s->move;
    BtlAiMgrAct *act = &s->act;
    f32 y = 0.0f;
    f32 x;
    BtlAiMgrTables *tbl = gBtlAi->data->tables;
    s32 hold;
    s32 cls;
    s32 state = BtlCharApi_GetUnk974(s->side);

    hold = BTLAI_BTN_DASH;
    if (m->mode == 2) {
        hold = BTLAI_BTN_DASH | BTLAI_BTN_CHARGE;
    }
    if (act->flags & BTLAI_ACT_BLOCKED_AHEAD) {
        hold |= BTLAI_BTN_ASCEND;
    }
    cls = tbl->stateClass[state];
    x = y;
    if (cls == 10) {
        y = -1.0f;
    }
    func_001BCB78(s, hold, 0, 0, 0, x, y);
}

/* Phase 0 of the move action: goes to phase 1 at once. */
void BtlAiMove_Init(BtlAiMgrSide *s) {
    s->act.phase = 1;
    BtlAiMove_Start(s);
}

/* Phase 1: reads mode and type from the action entry, picks a direction, computes the target, then runs. */
void BtlAiMove_Start(BtlAiMgrSide *s) {
    BtlAiMgrAct *act = &s->act;
    BtlAiMoveWork *m = &s->move;
    BtlAiMgrActEntry *e = &act->stack[act->depth - 1];
    s32 v;

    v = BtlAiMove_Div(e->arg, 10);
    func_001BC918(&s->out, 0);
    if (v == 1) {
        goto set;
    }
    if (v == 2) {
    set:
        m->mode = v;
    }
    v = BtlAiMove_Mod(e->arg, 10);
    m->type = v;
    m->dir = BtlAiMove_PickDir(s, m->mode);
    m->remain = gBtlAi->dist;
    act->flags &= ~BTLAI_ACT_PATH;
    m->stuckTimer = 0;
    BtlAiMove_CalcTarget(s);
    act->phase = 2;
    BtlAiMove_Run(s);
}

/* Phase 2: acts on BtlAiMove_Check and ends the action when asked to, after 61 frames without progress, or
   (type 4) once the fighters are further apart than dist[1]. */
void BtlAiMove_Run(BtlAiMgrSide *s) {
    BtlAiMoveWork *m = &s->move;
    BtlAiMgrAct *act = &s->act;
    BtlAiMovePath *path = &s->move.path;

    switch (BtlAiMove_Check(s)) {
    case 0:
        BtlAiMove_CalcTarget(s);
        act->phase = 1;
        break;
    case 1:
        act->phase = 3;
        break;
    case 2:
        act->flags &= ~BTLAI_ACT_PATH;
        func_001BC918(&s->out, 1);
        if (m->type == 0) {
            BtlAiMove_Hold(s);
        } else {
            BtlAiMove_Steer(s, &m->target, 1);
        }
        break;
    case 3:
        act->flags |= BTLAI_ACT_PATH;
        BtlAiMove_Steer(s, (BtlAiVec *)&path->pts[path->count - 1], 0);
        break;
    case 4:
        func_001BCB78(s, 0, 0, BTLAI_BTN_ASCEND, 0, 0.0f, 0.0f);
        break;
    }
    if (m->flags & 1) {
        act->phase = 3;
        m->flags &= ~1;
    }
    if (++m->stuckTimer > 60) {
        act->phase = 3;
    }
    if (m->type == 4) {
        if (gBtlAi->dist < m->dist[1]) {
            act->phase = 3;
        }
    }
}

/* Phase 3: releases the pad (pressing DASH in state class 10, holding ASCEND in states 0x1F..0x22), and
   pops the action once the fighter is in state class 0. */
void BtlAiMove_End(BtlAiMgrSide *s) {
    BtlAiMgrAct *act = &s->act;
    BtlAiMgrTables *tbl = gBtlAi->data->tables;
    s32 state = BtlCharApi_GetUnk974(s->side);
    s32 cls = tbl->stateClass[state];

    func_001BC918(&s->out, 1);
    if (cls == 10) {
        func_001BCB78(s, 0, BTLAI_BTN_DASH, 0, 0, 0.0f, 0.0f);
    }
    if ((u32)(state - 0x1F) < 4) {
        func_001BCB78(s, BTLAI_BTN_ASCEND, 0, 0, 0, 0.0f, 0.0f);
    }
    if (cls == 0) {
        act->depth--;
    }
}
