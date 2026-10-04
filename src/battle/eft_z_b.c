#include "common.h"
#define EFT_Z_IMPL
#include "battle/eft_z.h"

/*
 * Ground dust, kind 4 (landing ring), 0x198BC0..0x199500. Continues eft_z.c; see include/battle/eft_z.h.
 * A separate file only because EftGndDustLand_Init is INCLUDE_ASM and owns float constants: placed first, its
 * constants (0x2FCDCC..0x2FCE20) stay in the assembly .lit4 chunk in front of this file's (0x2FCE20, 0x2FCE24).
 * With the attempt enabled this file's .lit4 is 0x2FCDCC..0x2FCE28, identical to the original except that
 * 0x2FCDF4 and 0x2FCDF8 come out swapped.
 */

/* Kind 4 init: a ring of three low puffs, eight particles thrown outwards and one in the middle. */
#if 0
/* NON-MATCHING: the logic is the original's (checked instruction by instruction against the disassembly), the register
   allocation is not: the original keeps &arg->colA / &arg->colB in s1 / s0 until the two colour copies are made and
   spills fresh copies (sp+0x84 / sp+0x88) for the second loop; here one stack slot holds each pointer from the
   start, and the saved float registers are assigned differently. About 300 of 394 instructions differ. */
void EftGndDustLand_Init(EftZTask *task, EftGndDustArg *arg) {
    EftGndDustEmit *w = task->work;
    u8 *srcA = arg->colA;
    u8 *srcB = arg->colB;
    Vec4 pos;
    Vec4 dir;
    Vec4 v;
    Vec4 off;
    EftZCol colA __attribute__((aligned(16)));
    EftZCol colB __attribute__((aligned(16)));
    s32 i;
    s16 life;
    s16 fade;
    f32 start;
    f32 step = 2.0943951f;
    f32 ang;
    f32 s;
    f32 spin;

    memset(w, 0, sizeof(EftGndDustEmit));
    List_Init(&w->parts);
    w->free = EftGndDust_GetFreeList(arg->pool);
    EftGndDust_GetLightColors(srcA, srcB);
    Vec4_Set(V(&arg->accel), 0.0f, -1.0f, 0.0f, 1.0f);
    arg->pool = 1;
    arg->tex = 2;
    arg->life = 30;
    arg->fade = 10;
    arg->size = 10;
    arg->grow = 0.1f;
    arg->blend = 0;
    arg->rMin = 0;
    arg->rMax = 0;
    arg->spin = 0;
    arg->speed = 0.0f;
    arg->unk54 = 0.0f;
    arg->drag = 1.0f;
    arg->unk5C = 0.0f;
    start = RANDF() * 6.2831853f;
    colA = *(EftZCol *)srcA;
    colB = *(EftZCol *)srcB;
    for (i = 0; i < 3; i++) {
        ang = EftMath_WrapAngle(start + step * i);
        s = Mathf_SinFast(ang);
        Vec4_Set(&dir, s, 0.0f, Mathf_CosFast(ang), 1.0f);
        Vec3_Normalize(&dir, &dir);
        Vec4_Scale(&off, &dir, 2.0f);
        Vec4_Add(&pos, V(&arg->pos), &off);
        func_00122190(&v, V(&arg->dir), &dir, RANDF() * 0.2f + 0.6f);
        EftGndDust_SpawnPieceEx(w, &pos, &v, colA.c, colB.c, (f32)arg->life, (f32)arg->fade, 10, 5.0f, 6.0f, 0.0f, 1.0f, 0.0f,
                      1.0f, 0.0f, 0.0f, 0.7f, 0.0f, EFT_GDUST_PART_FAR);
    }
    start = RANDF() * 6.2831853f;
    step = 0.78539816f;
    for (i = 0; i < 8; i++) {
        ang = EftMath_WrapAngle(start + step * i);
        s = Mathf_SinFast(ang);
        Vec4_Set(&dir, s, 0.0f, Mathf_CosFast(ang), 1.0f);
        Vec3_Normalize(&dir, &dir);
        Vec4_Scale(&off, &dir, 2.0f);
        Vec4_Add(&pos, V(&arg->pos), &off);
        pos.y -= 3.0f;
        life = (f32)(arg->life + rand() % 10);
        fade = (f32)(arg->fade + rand() % 5);
        spin = RANDF() * 6.2831853f;
        EftGndDust_SpawnPieceEx(w, &pos, &dir, arg->colA, arg->colB, life, fade, 350, 1.0f, 1.0f, 0.0f, 0.0f,
                      arg->scale * 7.8f, 0.8f, spin, 0.3f, arg->scale * 0.7f,
                      (RANDF() * 0.01f + 0.07f) * arg->scale, 0);
    }
    spin = RANDF() * 6.2831853f;
    EftGndDust_SpawnPieceEx(w, V(&arg->pos), V(&arg->dir), arg->colA, arg->colB, life, fade, 350, 1.0f, 1.0f, 0.0f, 0.0f,
                  arg->scale * 0.8f, 0.8f, spin, 0.3f, arg->scale * 0.7f, (RANDF() * 0.01f + 0.03f) * arg->scale, 0);
    w->arg = *arg;
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/eft_z_b", EftGndDustLand_Init);

/* Kind 4 term. */
void EftGndDustLand_Term(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    EftGndDust_FreeParts(w);
}

/* Kind 4 reset: kills the task. */
void EftGndDustLand_Reset(EftZTask *task) {
    func_001ADA58(task);
}

/* Kind 4 update: follows the owner; the task ends with its last particle. */
void EftGndDustLand_Update(EftZTask *task) {
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
        w->texFar = EftGndDust_GetTex(gEftGndDust->texPtr, 2);
    } else {
        func_001ADA58(task);
    }
}

/* Kind 4 draw: oriented quads; the low puffs fade in between 80 and 150 units from the camera. */
void EftGndDustLand_Draw(EftZTask *task) {
    EftGndDustEmit *w = task->work;
    EftGndDustPart *p = (EftGndDustPart *)List_GetHead(&w->parts);
    Vec4 color;
    Vec4 pos;
    Vec4 d;
    s32 far;
    f32 fade;
    f32 dist;

    func_00120AB0();
    func_00120B80(&gBtlCamView->world2screen);
    while (p != NULL) {
        far = 0;
        if (p->flags & EFT_GDUST_PART_FAR) {
            far = 1;
        }
        fade = 1.0f;
        Vec3_Add(&pos, &p->pos, &p->base);
        if (far) {
            Vec3_Sub(&d, &pos, &gBtlCamView->eye);
            dist = func_001221E0(&d);
            fade = (dist - 6400.0f) / 16100.0f;
            if (22500.0f < dist) {
                fade = 1.0f;
            }
            if (dist < 6400.0f) {
                fade = 0.0f;
            }
        }
        Vec4_Set(&color, p->color.x * w->arg.bright, p->color.y * w->arg.bright, p->color.z * w->arg.bright,
                 p->color.w * p->alpha * fade);
        EftGndDust_DrawPiece(&pos, &color, &p->dir, w->arg.blend, (f32)p->scale * w->arg.scale * p->size.x,
                      (f32)p->scale * w->arg.scale * p->size.y, p->angle, far ? w->texFar : w->tex, far);
        p = (EftGndDustPart *)List_GetNext(&p->link);
    }
    func_00120AC8();
}
