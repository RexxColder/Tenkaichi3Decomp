#include "common.h"
#define EFT_Z_IMPL
#include "battle/eft_z.h"

/*
 * Ground dust, kind 5 (impact) and the particle helpers, 0x199500..0x199F28. Continues eft_z_b.c; see
 * include/battle/eft_z.h. A separate file only because EftGndDustImpact_Init is INCLUDE_ASM and owns float
 * constants: placed first, its constants (0x2FCE28..0x2FCE50) stay in the assembly .lit4 chunk in front of this
 * file's one constant (0x2FCE50). With the attempt enabled this file's .lit4 is 0x2FCE28..0x2FCE54, identical to
 * the original.
 */

/* Kind 5 init: two particles thrown outwards from the point. */
#if 0
/* NON-MATCHING: 68 of 250 instructions, all before the loop (the loop body and the tail are identical). The original
   keeps &arg->colA / &arg->colB in s0 / s1 for the EftGndDust_GetLightColors call and copies them to sp+0x50 / sp+0x54 later;
   here each pointer lives in its stack slot from the start, which also shifts the prologue and the order of the
   argument stores. */
void EftGndDustImpact_Init(EftZTask *task, EftGndDustArg *arg) {
    EftGndDustEmit *w = task->work;
    Vec4 pos;
    Vec4 dir;
    Vec4 off;
    u8 *colA;
    u8 *colB;
    s32 i;
    s32 life;
    s32 fade;
    f32 start;
    f32 step;
    f32 ang;
    f32 s;
    f32 spin;
    f32 grow;

    memset(w, 0, sizeof(EftGndDustEmit));
    List_Init(&w->parts);
    w->free = EftGndDust_GetFreeList(arg->pool);
    EftGndDust_GetLightColors(arg->colA, arg->colB);
    arg->scale = BtlScene_GetCharScale(arg->chr);
    if (arg->scale < 0.8f) {
        arg->scale = 0.8f;
    }
    Vec4_Set(V(&arg->accel), 0.0f, -1.0f, 0.0f, 1.0f);
    colA = arg->colA;
    arg->pool = 1;
    arg->tex = 2;
    colB = arg->colB;
    arg->grow = 0.1f;
    arg->size = 10;
    arg->life = 15;
    arg->fade = 5;
    arg->unk5C = 0.0f;
    arg->blend = 0;
    arg->rMin = 0;
    arg->rMax = 0;
    arg->spin = 0;
    arg->speed = 0.0f;
    arg->unk54 = 0.0f;
    arg->drag = 1.0f;
    step = 2.0943951f;
    start = RANDF() * 6.2831853f;
    for (i = 0; i < 2; i++) {
        ang = EftMath_WrapAngle(start + step * i);
        s = Mathf_SinFast(ang);
        Vec4_Set(&dir, s, 0.0f, Mathf_CosFast(ang), 1.0f);
        Vec3_Normalize(&dir, &dir);
        Vec4_Scale(&off, &dir, 2.0f);
        Vec4_Add(&pos, V(&arg->pos), &off);
        pos.y -= RANDF() * 3.0f;
        life = arg->life + rand() % 5;
        fade = arg->fade + rand() % 5;
        spin = RANDF() * 6.2831853f;
        grow = (RANDF() * 0.005f + 0.005f) * arg->scale;
        EftGndDust_SpawnPieceEx(w, &pos, &dir, colA, colB, (f32)life, (f32)fade, rand() % 10 + 25, 1.0f, 1.0f, 0.05f,
                      0.05f, arg->scale * 0.3f, 0.8f, spin, 0.3f, 1.0f, grow, 0);
    }
    w->arg = *arg;
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/eft_z_c", EftGndDustImpact_Init);

/* Kind 5 term. */
void EftGndDustImpact_Term(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    EftGndDust_FreeParts(w);
}

/* Kind 5 reset: kills the task. */
void EftGndDustImpact_Reset(EftZTask *task) {
    BtlTask_SetDead(task);
}

/* Kind 5 update: follows the owner; the task ends with its last particle. */
void EftGndDustImpact_Update(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    if (!BtlScene_IsTimeStopped()) {
        BtlCharApi_GetPos(w->arg.chr, &w->pos);
        BtlCharApi_GetDir(w->arg.chr, &w->dir);
        Vec3_Scale(&w->dir, &w->dir, -1.0f);
        Vec3_Normalize(&w->dir, &w->dir);
        EftGndDust_UpdateParts(w);
    }
    if (List_GetHead(&w->parts) != NULL) {
        w->tex = EftGndDust_GetTex(gEftGndDust->texPtr, 0);
    } else {
        BtlTask_SetDead(task);
    }
}

/* Kind 5 draw: oriented quads at the particles' own positions. */
void EftGndDustImpact_Draw(EftZTask *task) {
    EftGndDustEmit *w = task->work;
    EftGndDustPart *p = (EftGndDustPart *)List_GetHead(&w->parts);
    Vec4 color;

    func_00120AB0();
    func_00120B80(&gBtlCamView->world2screen);
    while (p != NULL) {
        Vec4_Set(&color, p->color.x * w->arg.bright, p->color.y * w->arg.bright, p->color.z * w->arg.bright,
                 p->color.w * p->alpha);
        EftGndDust_DrawPiece(&p->pos, &color, &p->dir, w->arg.blend, (f32)p->scale * w->arg.scale * p->size.x,
                      (f32)p->scale * w->arg.scale * p->size.y, p->angle, w->tex, 0);
        p = (EftGndDustPart *)List_GetNext(&p->link);
    }
    func_00120AC8();
}

/* Steps every particle of a task: life, attachment to the owner, colour, size, spin, velocity, fade-out; frees
   the finished ones. */
void EftGndDust_UpdateParts(EftGndDustEmit *w) {
    EftGndDustPart *p = (EftGndDustPart *)List_GetHead(&w->parts);

    while (p != NULL) {
        p->life--;
        if (p->life <= 0) {
            p->life = 0;
            p->flags |= EFT_GDUST_PART_FADING;
        }
        if (p->flags & EFT_GDUST_PART_FOLLOW) {
            Vec4_Copy(&p->base, &w->pos);
        }
        if (p->flags & EFT_GDUST_PART_TURN) {
            Vec4_Copy(&p->dir, &w->dir);
            Vec3_Scale(&p->vel, &p->dir, p->speed);
        }
        EftGndDust_LerpColor(&p->color, p->colA, p->colB, (f32)p->life / (f32)p->lifeMax);
        Vec3_Add(&p->size, &p->size, &p->grow);
        Vec3_Scale(&p->grow, &p->grow, p->growDamp);
        p->angle += p->spin * 3.14159265f / 180.0f;
        p->angle = EftMath_WrapAngle(p->angle);
        Vec3_Scale(&p->vel, &p->vel, p->drag);
        Vec3_Add(&p->vel, &p->vel, &p->accel);
        Vec3_Add(&p->pos, &p->pos, &p->vel);
        if (w->flags & EFT_GDUST_DROP) {
            if (w->partFlags & EFT_GDUST_PART_FOLLOW) {
                p->flags &= ~EFT_GDUST_PART_FOLLOW;
                Vec3_Add(&p->pos, &p->pos, &p->base);
                Vec4_Set(&p->base, 0.0f, 0.0f, 0.0f, 1.0f);
            }
        }
        if (p->flags & EFT_GDUST_PART_FADING) {
            do {
                if (p->fade > 0) {
                    p->fade--;
                    p->alpha = (f32)p->fade / (f32)p->fadeLen;
                    if (p->fade > 0) {
                        break;
                    }
                    p->alpha = 0.0f;
                }
                p->flags |= EFT_GDUST_PART_DONE;
            } while (0);
        }
        if (p->flags & EFT_GDUST_PART_DONE) {
            p->flags &= ~EFT_GDUST_PART_ALIVE;
            p = EftGndDust_MoveNode(&w->parts, w->free, p);
            gEftGndDust->count--;
        } else {
            p = (EftGndDustPart *)List_GetNext(&p->link);
        }
    }
}

/* out = a * t + b * (1 - t), per byte. */
void EftGndDust_LerpColor(Vec4 *out, u8 *a, u8 *b, f32 t) {
    f32 u = 1.0f - t;

    out->x = a[0] * t + b[0] * u;
    out->y = a[1] * t + b[1] * u;
    out->z = a[2] * t + b[2] * u;
    out->w = a[3] * t + b[3] * u;
}

/* The free list of particle pool 0 / 1. */
List *EftGndDust_GetFreeList(s32 pool) {
    return &gEftGndDust->free[pool];
}

/* Takes a particle from a pool, links it into a task's list and clears it. */
EftGndDustPart *EftGndDust_AllocPart(List *list, List *free) {
    EftGndDustPart *p = (EftGndDustPart *)List_PopFront(free);
    ListNode *prev;
    ListNode *next;

    if (p != NULL) {
        List_PushBack(list, &p->link);
        prev = p->link.prev;
        next = p->link.next;
        memset(p, 0, sizeof(EftGndDustPart));
        p->link.prev = prev;
        p->link.next = next;
        return p;
    }
    return p;
}
