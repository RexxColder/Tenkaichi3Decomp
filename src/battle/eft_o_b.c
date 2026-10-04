#include "common.h"
#include "battle/eft_o.h"

/*
 * The blast object, 0x1699D0..0x16AE78: one flying shot of a multi-shot technique. SIMULATION.
 *
 * A blast object is a task of class 0x2C3AD8 (gEftBlastObjClass) in a list of 60 (work block 0x620 bytes,
 * EftBlastObj) owned by the manager class 0x2C3AC0. It is created by the technique effect modules that fire
 * several shots: EftShotTech (type 2, eft_j.c / eft_k.c, up to 14), EftVolley (type 1, eft_h.c, up to 30) and
 * EftRingShot (type 4, eft_l_b.c, up to 20). The creator keeps the task pointer as a handle and steers the
 * object through the entry points at 0x16A900..0x16AE30; every entry point first checks that the handle is
 * not NULL and that the task's class update is EftBlastObj_Update, then that the object is alive (flag 1).
 * (A handle whose task was killed and reused by another blast object passes that test: the creators drop
 * their handle when EftBlastObj_IsAlive fails, which they poll every frame.)
 *
 * LIFE OF A BLAST OBJECT (verified by the matching code below, except what EftBlastObj_Init and
 * EftBlastObj_UpdateParts do: those two are left in assembly, 6 and 70 instructions away from their C, and were
 * checked against the disassembly line by line)
 *
 * Creation (EftBlastObj_Create / EftBlastObj_CreateWithModel -> EftBlastObj_Init):
 *   - position = *arg->pos (also kept as the record's "start"), direction = *arg->dir (taken as given, not
 *     normalised here), velocity = direction * arg->speed, scale = arg->scale;
 *   - previous position = the OWNER's node 0x11, so the first hit shape is swept from the owner's body;
 *   - life: arg->life seconds * 30 frames when positive (flag TIMED), else unlimited;
 *   - delay = 1 frame: it does not move on the update of the frame it is created in;
 *   - the creator's node slots are copied; when sel[0].node is 0..4 the matching slot is given the fighter
 *     model node that the animation attribute of event {2, 4, 8, 0x10, 0x20}[slot] names (EftShot_GetAttrKind);
 *   - the task gets class flag 0x800 (owner is object 0) or 0x1000 (any other owner), the bits BtlScene_Reset
 *     uses to reset one character's effects.
 *   No random number is drawn anywhere in this module.
 *
 * Update, once per frame, skipped entirely while BtlScene_IsCharStopped(owner) (EftBlastObj_Update):
 *   1. model animation trigger: with flag ANIM, the owner's FIRE event (4) sets FIRED;
 *   2. the node slots are refreshed (EftEmit_UpdateNodesReq);
 *   3. motion, only when delay <= 0, the definition's flag 2 is clear, and the object is neither STUCK nor
 *      FROZEN: prev = pos; then either pos = target (flag PLACED, the owner writes the position), or
 *      optional homing (definition homing > 0: EftAim_Home turns dir toward the owner's opponent by at most
 *      `homing` per frame, velocity = dir * speed) followed by pos += velocity;
 *   4. delay -= 1 while positive (so a delay of n frames holds the object for n updates);
 *   5. life: with TIMED, lifeLeft -= 1 and STOP when it reaches 0; the owner's ABORT event (0x400) also
 *      sets STOP;
 *   6. the model, if any, is placed and animated (visual);
 *   7. the pack parts selected by sel[] are driven (visual);
 *   8. death and the hit record: DYING -> the task is killed; else STOP -> DYING (so a stopped object lives
 *      one more update, without a record); else, unless NO_HIT, the pack's trail width animation is stepped
 *      and ONE HIT RECORD is published (EftBlastObj_AddHit).
 *
 * Hit record (EftBlastObj_AddHit): a technique record (source = the owner's shot slot, so type, level and
 * damage come from the technique definition in EftHit_Add). start / pos / prev / vel are copied from the
 * object; the shape is, by the definition's shape byte, two spheres at pos and prev or two boxes from start
 * to pos and from start to prev (any other value: no record); radius = scale * the pack's trail width
 * animation (EftEmit_GetTrailWidth), so that "visual" animation track is simulation input. Record flag 0x80
 * (EFT_HIT_FLAG_NO_MODE1) is set while the object is HELD and 0x10 (EFT_HIT_FLAG_LAST, the last hit of the
 * technique) when it was marked LAST.
 *
 * Hit results (EftBlastObj_PostUpdate, also skipped while the owner is stopped): the hit pass leaves a result
 * in the task (EftHit_CalcResult: 0 none, 1 forced, 3, 4 hit, 5 finished). The object reads it as bits:
 *   - bit 0 (results 1, 3, 5): the position is replaced by task->pos, where the record was stopped (stage,
 *     guard, clash ...), and the object becomes STUCK: it never moves again;
 *   - bit 2 (results 4, 5): STOP, so it publishes no further record and dies on the update after the next.
 *   Result 3 therefore pins the object in place but lets it keep publishing records until its life ends.
 *   The post-update also drops the parts that have finished (EftEmit_UpdateAlive).
 *
 * Reset callback (BtlScene_Reset for the owner's bit): kills the pack parts and the task at once.
 *
 * Steering by the owner: EftBlastObj_SetTarget(on) makes the owner write the position each frame (the ring
 * shot's bobbing ring, the volley's path); EftBlastObj_SetDir writes the direction only: the VELOCITY IS NOT
 * RECOMPUTED there, it follows the direction only inside the homing branch (definition homing > 0), so a shot
 * of a non-homing technique keeps the velocity of its creation whatever direction it is given later.
 * EftBlastObj_SetDelay holds it for n frames; EftBlastObj_SetHeld marks its records; EftBlastObj_SetFrozen /
 * SetNoHit / SetModelAnim are used together by the volley's model shots (techniques 0x26E / 0x26F): frozen
 * and harmless until the owner's FIRE event, then released by the model animation steps.
 *
 * Order dependence: records are appended in task list order = creation order; the list is shared by both
 * fighters. Homing reads the opponent's position of the moment (see EftAim_Home in eft_a.h).
 * Non-simulation inputs: none. The model and the pack parts are appearance only, except the trail width
 * animation named above.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Asin(f32 x);

extern void Vec4_Copy(void *dst, void *src);
extern void Vec3_Add(void *dst, void *a, void *b);
extern void Vec3_Scale(void *dst, void *src, f32 scale);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(void *dst, Mtx44 *m, void *src);
extern void func_00120150(Mtx44 *dst, Mtx44 *src, void *pos);  /* translate */
extern void func_00120398(Mtx44 *dst, Mtx44 *src, f32 angle);  /* rotate about X */
extern void func_00120428(Mtx44 *dst, Mtx44 *src, f32 angle);  /* rotate about Y */
extern void func_00120590(Mtx44 *dst, Mtx44 *src, f32 scale);  /* uniform scale */
extern void func_00121FB8(void *dst, void *src);               /* copies a position */
extern void func_00122140(void *dst, void *src, f32 lo, f32 hi); /* clamp x, y, z */
extern f32 EftMath_WrapAngle(f32 angle);
extern void EftAim_Home(void *out, void *pos, void *dir, s32 objId, f32 speed, f32 maxTurn);

extern void *BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(void *pool, s32 size);
extern void BtlPool_Free(void *pool, void *p);
extern s32 BtlScene_IsCharStopped(s32 objId);
extern void *BtlTask_CreateChildList(EftOTask *task, s32 count, s32 workSize);
extern EftOTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void func_001ADA58(EftOTask *task);            /* kill a task */
extern void func_001ADB78(EftOTask *task, s32 flags); /* or into the task's class flags */

extern void BtlCharApi_GetNodePos(s32 objId, s32 node, void *out);

extern EftOHitRec *EftHit_GetNew(void);
extern void EftHit_Add(EftOHitRec *rec);
extern void *EftHitArena_AllocSphere(void);
extern void *EftHitArena_AllocBox(void);
extern void EftHit_SetShapeSpheres(EftOHitRec *rec, void *a, void *b);
extern void EftHit_SetShapeBoxes(EftOHitRec *rec, void *a, void *b);
extern void func_00239588(void *box, void *from, void *to, f32 radius);
extern void func_002399A0(void *sphere, void *center, f32 radius);

extern s32 EftShot_TestBits(s32 objId, s32 mask);
extern s32 EftShot_GetAttrKind(s32 objId, s32 bits);
extern void EftEmit_InitState(EftOSet *set, void *state);
extern void EftEmit_TermState(EftOSet *set, void *state);
extern s32 EftEmit_GetFlagsFromReq(EftOSet *set, void *state, s32 objId, s32 type, s32 idx, s32 ending, s32 kill);
extern s32 EftEmit_GetResetFlags(EftOSet *set, void *state, s32 type, s32 idx);
extern void EftEmit_SpawnOwn(EftOSet *set, void *state, EftOEmitNodes *nodes, void *pos, void *dir, s32 type,
                             s32 idx, s32 flags, f32 scale);
extern void EftEmit_KillAll(EftOSet *set, void *state);
extern s32 EftEmit_UpdateAlive(EftOSet *set, void *state);
extern void EftEmit_UpdateNodesReq(EftOSrc *src, EftOEmitNodes *nodes);
extern void EftEmit_UpdateTrailWidth(EftOSet *set, void *state);
extern f32 EftEmit_GetTrailWidth(void *state);

extern s32 EftObj_Create(void *arg, s32 *pack);       /* creates a battle object for a model */
extern void EftObj_Destroy(s32 obj);                   /* destroys it */
extern void EftObj_PlayAnim(s32 obj, s32 anim, s32 mode); /* starts an animation of it */
extern s32 EftObj_IsAnimPlaying(s32 obj);                    /* its animation is still playing */
extern void EftObj_SetMtx(s32 obj, Mtx44 *m);         /* places it */
extern void EftObj_SetVisible(s32 obj, s32 show);
extern void EftObj_Nop(s32 obj, s32 a1);

extern void *gEftBlastObjClass[6];

/* Publishes this frame's hit record of a blast object. */
void EftBlastObj_AddHit(EftOTask *task) {
    EftBlastObj *w = task->work;
    EftOHitRec *rec = EftHit_GetNew();
    EftOSrc *src = w->src;
    f32 r;

    r = w->scale * EftEmit_GetTrailWidth(w->state);
    rec->pose = w->pose;
    rec->task = task;
    rec->src = src;
    if (w->flags & EFT_BLASTOBJ_HELD) {
        rec->flags |= 0x80;
    }
    if (w->flags & EFT_BLASTOBJ_LAST) {
        rec->flags |= 0x10;
    }
    switch (src->def->shape) {
    case 1: {
        void *a = EftHitArena_AllocBox();
        void *b = EftHitArena_AllocBox();

        func_00239588(a, &w->pose.start, &w->pose.pos, r);
        func_00239588(b, &w->pose.start, &w->pose.prev, r);
        EftHit_SetShapeBoxes(rec, a, b);
        break;
    }
    case 0: {
        void *a = EftHitArena_AllocSphere();
        void *b = EftHitArena_AllocSphere();

        func_002399A0(a, &w->pose.pos, r);
        func_002399A0(b, &w->pose.prev, r);
        EftHit_SetShapeSpheres(rec, a, b);
        break;
    }
    default:
        return;
    }
    EftHit_Add(rec);
}

/* Creates the battle object that shows the blast object's model, hidden. */
void EftBlastObj_InitModel(EftOTask *task) {
    EftBlastObjModel *m = &((EftBlastObj *)task->work)->model;

    m->obj = EftObj_Create(m->arg, m->pack);
    m->step = -1;
    EftObj_SetVisible(m->obj, 0);
    EftObj_Nop(m->obj, 0);
}

/* Places the model along the direction of flight and, with flag ANIM, plays its animation steps: step 0 at
   once, step 1 when step 0 is over (looping), step 2 at the owner's FIRE event (the object starts to hit),
   step 3 when step 2 is over (the object starts to move), step 4 when step 3 is over (looping). Visual, except
   that the steps clear the NO_HIT and FROZEN flags. */
void EftBlastObj_UpdateModel(EftOTask *task) {
    Mtx44 mtx;
    Vec4 dir;
    Vec4 pos;
    s32 change = 0;
    s32 mode = 1;
    EftBlastObj *w = task->work;
    EftBlastObjModel *m;
    f32 pitch;
    f32 yaw;

    Mtx_StoreIdentity(&mtx);
    func_00120590(&mtx, &mtx, w->scale);
    m = &w->model;
    if (w->flags & EFT_BLASTOBJ_ANIM) {
        if (m->step < 0) {
            if (m->time >= 0.0f) {
                change = 1;
                m->step++;
            }
        } else if (m->step < 1) {
            if (!EftObj_IsAnimPlaying(m->obj)) {
                change = 1;
                m->time = 0.0f;
                mode = 2;
                m->step++;
            }
        } else if (m->step < 2) {
            if (w->flags & EFT_BLASTOBJ_FIRED) {
                m->time = 0.0f;
                m->step++;
                change = 1;
                w->flags &= ~EFT_BLASTOBJ_NO_HIT;
            }
        } else if (m->step < 3) {
            if (!EftObj_IsAnimPlaying(m->obj)) {
                m->step++;
                m->time = 0.0f;
                change = 1;
                w->flags &= ~EFT_BLASTOBJ_FROZEN;
            }
        } else if (m->step < 4) {
            if (!EftObj_IsAnimPlaying(m->obj)) {
                change = 1;
                m->time = 0.0f;
                mode = 2;
                m->step++;
            }
        }
        if (change) {
            EftObj_PlayAnim(m->obj, m->step, mode);
        }
        func_00122140(&dir, &w->dir, -1.0f, 1.0f);
        if (!(w->flags & EFT_BLASTOBJ_FROZEN)) {
            func_00120398(&mtx, &mtx, EftMath_WrapAngle(Mathf_Asin(dir.y)));
        }
        func_00120428(&mtx, &mtx, EftMath_WrapAngle(atan2f(dir.x, dir.z) + 3.14159265f));
        Vec4_Copy(&pos, &w->pose.pos);
        if (!(w->flags & EFT_BLASTOBJ_FROZEN)) {
            EftOVec ofs = { 0.0f, 5.5f, 0.0f, 1.0f };

            if (w->src->def->id == 0x26E) {
                ofs.y = w->scale * 5.5f;
            } else if (w->src->def->id == 0x26F) {
                ofs.y = w->scale * 7.0f;
            }
            Mtx_MulVec4(&ofs, &mtx, &ofs);
            Vec3_Add(&pos, &pos, &ofs);
        }
        func_00120150(&mtx, &mtx, &pos);
    } else {
        func_00122140(&dir, &w->dir, -1.0f, 1.0f);
        pitch = Mathf_Asin(dir.y);
        yaw = atan2f(dir.x, dir.z) + 3.14159265f;
        pitch = EftMath_WrapAngle(pitch);
        func_00120428(&mtx, &mtx, EftMath_WrapAngle(yaw));
        func_00120398(&mtx, &mtx, pitch);
        func_00120150(&mtx, &mtx, &w->pose.pos);
    }
    EftObj_SetMtx(m->obj, &mtx);
    EftObj_SetVisible(m->obj, 1);
    m->time += 1.0f;
}

/* Destroys the model's battle object. */
void EftBlastObj_TermModel(EftOTask *task) {
    EftObj_Destroy(((EftBlastObj *)task->work)->model.obj);
}

/* Drives the parts of the effect pack this object owns: those whose kind and node slot match one of its
   sel[] pairs (a part on node slot 5 matches any pair of its kind). Visual. */
/* NON-MATCHING: 70 of 115 instructions, same logic: the original branches to the loop exit from both node
   tests (`beql ..., exit` with `skip = 0` in the delay slot) and falls through to `i++`; this falls through
   to `skip = 0`. The loop counter and the part count also swap registers (a3 / t0). */
#if 0
void EftBlastObj_UpdateParts(s32 objId, EftOTask *task, EftOSet *set, s32 reset) {
    EftBlastObj *w = task->work;
    s32 g;
    s32 p;

    for (g = 0; g < 19; g++) {
        if (*set->mask & (1 << g)) {
            EftOGroupDef *gd = set->grp[g].def;

            for (p = 0; p < gd->count; p++) {
                s32 idx = set->grp[g].first + p;
                s32 skip = 1;
                s32 i;
                s32 f;

                for (i = 0; i < w->selCount; i++) {
                    if (set->parts[idx].kind == w->sel[i].kind) {
                        if (set->parts[idx].node == w->sel[i].node || set->parts[idx].node == 5) {
                            skip = 0;
                            break;
                        }
                    }
                }
                if (skip) {
                    continue;
                }
                if (reset == 0) {
                    f = EftEmit_GetFlagsFromReq(set, w->state, objId, g, p, w->flags & EFT_BLASTOBJ_STOP,
                                                w->flags & 8);
                } else {
                    f = EftEmit_GetResetFlags(set, w->state, g, p);
                }
                if (f != 0) {
                    EftEmit_SpawnOwn(set, w->state, &w->nodes, &w->pose.pos, &w->dir, g, p, f, w->scale);
                }
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/eft_o_b", EftBlastObj_UpdateParts);
#endif

/* Init callback: see the notes at the top of the file. */
/* NON-MATCHING: 6 instructions, register allocation only, in the copy of the node slots: the original keeps
   the end pointer of the block copy in v0 and copies through t1 / v1 / a1 / a2; this keeps it in a1 and
   copies through t1 / v0 / v1 / a2. */
#if 0
void EftBlastObj_Init(EftOTask *task, EftBlastObjArg *arg) {
    EftBlastObj *w = task->work;
    EftOSrc *src;
    s32 slot;

    memset(w, 0, sizeof(EftBlastObj));
    w->set = arg->set;
    w->src = src = arg->src;
    w->posArg = arg->pos;
    w->dirArg = arg->dir;
    w->index = arg->index;
    w->count = arg->count;
    w->life = arg->life;
    w->scaleArg = arg->scale;
    w->speed = arg->speed;
    w->sel[0].kind = arg->kind;
    w->sel[0].node = arg->evtIdx;
    w->selCount = 1;
    {
        s32 slots[7] = { 0, 1, 3, 4, 5, -1, -1 };
        s32 bits[6] = { 2, 4, 8, 0x10, 0x20, 0x40 };

        slot = slots[w->sel[0].node];
        w->nodes = *arg->nodes;
        if (slot >= 0) {
            s32 node = EftShot_GetAttrKind(src->objId, bits[slot]);

            w->nodes.n[slot][0].id = node;
            BtlCharApi_GetNodePos(src->objId, node, &w->nodes.n[slot][0].pos);
        }
    }
    w->flags |= EFT_BLASTOBJ_ALIVE;
    if (w->life > 0.0f) {
        w->flags |= EFT_BLASTOBJ_TIMED;
        w->lifeLeft = w->life * 30.0f;
    }
    w->scale = w->scaleArg;
    Vec4_Copy(&w->pose.start, w->posArg);
    Vec4_Copy(&w->pose.pos, &w->pose.start);
    Vec4_Copy(&w->dir, w->dirArg);
    BtlCharApi_GetNodePos(src->objId, 0x11, &w->pose.prev);
    Vec3_Scale(&w->pose.vel, &w->dir, w->speed);
    w->delay = 1.0f;
    EftEmit_InitState(w->set, w->state);
    if (arg->model != NULL) {
        w->flags |= EFT_BLASTOBJ_MODEL;
        w->model.pack = arg->model;
        EftBlastObj_InitModel(task);
    }
    func_001ADB78(task, src->objId == 0 ? 0x800 : 0x1000);
}
#else
INCLUDE_RODATA("asm/nonmatchings/battle/eft_o_b", D_002ECC60); /* slots[7] = { 0, 1, 3, 4, 5, -1, -1 } */
INCLUDE_RODATA("asm/nonmatchings/battle/eft_o_b", D_002ECC80); /* bits[6] = { 2, 4, 8, 0x10, 0x20, 0x40 } */
INCLUDE_ASM("asm/nonmatchings/battle/eft_o_b", EftBlastObj_Init);
#endif

/* Term callback. */
void EftBlastObj_Term(EftOTask *task) {
    EftBlastObj *w = task->work;

    if (w->flags & EFT_BLASTOBJ_MODEL) {
        EftBlastObj_TermModel(task);
    }
    EftEmit_TermState(w->set, w->state);
    w->flags = 0;
}

/* Update callback: see the notes at the top of the file. */
void EftBlastObj_Update(EftOTask *task) {
    EftBlastObj *w = task->work;
    EftOSrc *src = w->src;

    if (BtlScene_IsCharStopped(src->objId)) {
        return;
    }
    if ((w->flags & EFT_BLASTOBJ_ANIM) && EftShot_TestBits(src->objId, 4)) {
        w->flags |= EFT_BLASTOBJ_FIRED;
    }
    EftEmit_UpdateNodesReq(src, &w->nodes);
    if (w->delay <= 0.0f && !(src->def->flags & 2)) {
        if (!(w->flags & EFT_BLASTOBJ_STUCK)) {
            if (!(w->flags & EFT_BLASTOBJ_FROZEN)) {
                Vec4_Copy(&w->pose.prev, &w->pose.pos);
                if (w->flags & EFT_BLASTOBJ_PLACED) {
                    Vec4_Copy(&w->pose.pos, &w->target);
                } else {
                    if (0.0f < src->def->homing) {
                        EftAim_Home(&w->dir, &w->pose.pos, &w->dir, src->objId, w->speed, src->def->homing);
                        Vec3_Scale(&w->pose.vel, &w->dir, w->speed);
                    }
                    Vec3_Add(&w->pose.pos, &w->pose.pos, &w->pose.vel);
                }
            }
        }
    }
    if (0.0f < w->delay) {
        w->delay -= 1.0f;
    }
    if (w->flags & EFT_BLASTOBJ_TIMED) {
        w->lifeLeft -= 1.0f;
        if (w->lifeLeft <= 0.0f) {
            w->flags |= EFT_BLASTOBJ_STOP;
        }
    }
    if (EftShot_TestBits(src->objId, 0x400)) {
        w->flags |= EFT_BLASTOBJ_STOP;
    }
    if (w->flags & EFT_BLASTOBJ_MODEL) {
        EftBlastObj_UpdateModel(task);
    }
    EftBlastObj_UpdateParts(src->objId, task, w->set, 0);
    if (w->flags & EFT_BLASTOBJ_DYING) {
        func_001ADA58(task);
    } else if (w->flags & EFT_BLASTOBJ_STOP) {
        w->flags |= EFT_BLASTOBJ_DYING;
    } else if (!(w->flags & EFT_BLASTOBJ_NO_HIT)) {
        EftEmit_UpdateTrailWidth(w->set, w->state);
        EftBlastObj_AddHit(task);
    }
}

/* Post-update callback: reacts to what the hit pass left in the task. */
void EftBlastObj_PostUpdate(EftOTask *task) {
    EftBlastObj *w = task->work;

    if (BtlScene_IsCharStopped(w->src->objId)) {
        return;
    }
    EftEmit_UpdateAlive(w->set, w->state);
    if ((u16)(task->result & 1)) {
        func_00121FB8(&w->pose.pos, &task->pos);
        if (!(w->flags & EFT_BLASTOBJ_STUCK)) {
            w->flags |= EFT_BLASTOBJ_STUCK;
        }
    }
    if (task->result & 4) {
        w->flags |= EFT_BLASTOBJ_STOP;
    }
}

/* Reset callback: the pack parts and the task die at once. */
void EftBlastObj_Reset(EftOTask *task) {
    EftBlastObj *w = task->work;

    if (!(w->flags & EFT_BLASTOBJ_KILLED)) {
        w->flags |= EFT_BLASTOBJ_KILLED;
        EftEmit_KillAll(w->set, w->state);
    }
    func_001ADA58(task);
}

/* Draw callback: nothing (the pack parts and the model draw themselves). */
void EftBlastObj_Draw(EftOTask *task) {
}

/* Init callback of the manager: a list of 60 blast objects. */
void EftBlastObjMgr_Init(EftOTask *task) {
    s32 *mgr = BtlPool_Alloc(BtlPool_GetCurrent(), 4);

    gEftBlastObjMgr = mgr;
    *mgr = 0;
    gEftBlastObjList = BtlTask_CreateChildList(task, 60, sizeof(EftBlastObj));
}

/* Term callback of the manager. */
void EftBlastObjMgr_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftBlastObjMgr);
    gEftBlastObjMgr = NULL;
}

/* Update callback of the manager: nothing. */
void EftBlastObjMgr_Update(void) {
}

/* Reset callback of the manager: nothing. */
void EftBlastObjMgr_Reset(void) {
}

/* Creates a blast object without a model; returns its task or NULL when the list is full. */
void *EftBlastObj_Create(EftBlastObjArg *arg) {
    EftBlastObjArg a = *arg;

    a.model = NULL;
    return BtlTaskList_AddTail(gEftBlastObjList, gEftBlastObjClass, &a);
}

/* Creates a blast object that shows a model. */
void *EftBlastObj_CreateWithModel(EftBlastObjArg *arg, s32 *model) {
    EftBlastObjArg a = *arg;

    a.model = model;
    return BtlTaskList_AddTail(gEftBlastObjList, gEftBlastObjClass, &a);
}

/* Asks the object to end: it publishes no more records and dies on the update after the next. */
s32 EftBlastObj_Stop(EftOTask *task) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_BLASTOBJ_STOP;
    return 1;
}

/* Ends the object and kills its pack parts now; the next update kills the task. No caller. */
s32 EftBlastObj_Kill(EftOTask *task) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_BLASTOBJ_DYING;
    if (!(w->flags & EFT_BLASTOBJ_KILLED)) {
        w->flags |= EFT_BLASTOBJ_KILLED;
        EftEmit_KillAll(w->set, w->state);
    }
    return 1;
}

/* The handle is a live blast object. */
s32 EftBlastObj_IsAlive(EftOTask *task) {
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    if (((EftBlastObj *)task->work)->flags & EFT_BLASTOBJ_ALIVE) {
        return 1;
    }
    return 0;
}

/* The object is about to be killed. No caller. */
s32 EftBlastObj_IsDying(EftOTask *task) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    if (w->flags & EFT_BLASTOBJ_DYING) {
        return 1;
    }
    return 0;
}

/* Sets the n-th (part kind, node slot) pair that selects the pack parts this object drives. */
s32 EftBlastObj_SetSel(EftOTask *task, s32 n, s32 kind, s32 node) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    w->sel[n].kind = kind;
    w->sel[n].node = node;
    if (!(n < w->selCount)) {
        w->selCount++;
    }
    return 1;
}

/* The object's four hit record vectors, or NULL. No caller. */
EftOHitPose *EftBlastObj_GetPose(EftOTask *task) {
    EftBlastObj *w;

    if (task == NULL) {
        return NULL;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return NULL;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return NULL;
    }
    return &w->pose;
}

/* on: from now on the object is put at `pos` on every update that would move it (the owner calls this every
   frame with the new position). off: it flies by its own velocity again. */
s32 EftBlastObj_SetTarget(EftOTask *task, s32 on, Vec4 *pos) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    if (on) {
        w->flags |= EFT_BLASTOBJ_PLACED;
        Vec4_Copy(&w->target, pos);
    } else {
        w->flags &= ~EFT_BLASTOBJ_PLACED;
    }
    return 1;
}

/* Writes the previous position: the tail of the next hit shape. */
s32 EftBlastObj_SetPrevPos(EftOTask *task, Vec4 *pos) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    Vec4_Copy(&w->pose.prev, pos);
    return 1;
}

/* Writes the direction of flight. The velocity only follows when the object homes. */
s32 EftBlastObj_SetDir(EftOTask *task, Vec4 *dir) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    Vec4_Copy(&w->dir, dir);
    return 1;
}

/* Frozen: the object does not move (its model animation releases it). */
s32 EftBlastObj_SetFrozen(EftOTask *task, s32 on) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    if (on) {
        w->flags |= EFT_BLASTOBJ_FROZEN;
    } else {
        w->flags &= ~EFT_BLASTOBJ_FROZEN;
    }
    return 1;
}

/* No hit: the object publishes no hit record (its model animation arms it at the owner's FIRE event). */
s32 EftBlastObj_SetNoHit(EftOTask *task, s32 on) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    if (on) {
        w->flags |= EFT_BLASTOBJ_NO_HIT;
    } else {
        w->flags &= ~EFT_BLASTOBJ_NO_HIT;
    }
    return 1;
}

/* Held: the object's records carry EFT_HIT_FLAG_NO_MODE1 (0x80) until it is released. */
s32 EftBlastObj_SetHeld(EftOTask *task, s32 on) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    if (on) {
        w->flags |= EFT_BLASTOBJ_HELD;
    } else {
        w->flags &= ~EFT_BLASTOBJ_HELD;
    }
    return 1;
}

/* The model plays its animation steps (see EftBlastObj_UpdateModel). */
s32 EftBlastObj_SetModelAnim(EftOTask *task, s32 on) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    if (on) {
        w->flags |= EFT_BLASTOBJ_ANIM;
    } else {
        w->flags &= ~EFT_BLASTOBJ_ANIM;
    }
    return 1;
}

/* Marks the object as the technique's last shot: its records carry EFT_HIT_FLAG_LAST (0x10). */
s32 EftBlastObj_MarkLast(EftOTask *task) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    w->flags |= EFT_BLASTOBJ_LAST;
    return 1;
}

/* Frames before the object starts to move. */
s32 EftBlastObj_SetDelay(EftOTask *task, f32 frames) {
    EftBlastObj *w;

    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != (void *)EftBlastObj_Update) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_BLASTOBJ_ALIVE)) {
        return 0;
    }
    w->delay = frames;
    return 1;
}
