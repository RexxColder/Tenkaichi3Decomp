#include "common.h"
#include "battle/eft_v_ext.h"

/*
 * Effect code 0x187C50..0x1895E8: the first half of the sprite chain module, effect pack part kind 15 (see
 * include/battle/eft_v.h); the module continues at 0x1895E8 in another file.
 *
 * Drawing only. Read from the simulation: BtlScene_IsEffectStopped. Random numbers: the VU0 register through
 * Rand_FloatRange (EftLink_Place: one per sprite per frame; EftLink_InitGrow: three), appearance only.
 */

/* Manager init: the node pool and the list of 6 chain tasks. */
void EftLinkMgr_Init(EftVTask *task) {
    gEftLink = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftLinkMgr));
    memset(gEftLink, 0, sizeof(EftLinkMgr));
    gEftLink->list = BtlTask_CreateChildList(task, 6, sizeof(EftLinkWork));
    Mtx_StoreIdentity(&gEftLink->identity);
}

/* Manager update: nothing. */
void EftLinkMgr_Update(EftVTask *task) {
}

/* Manager reset: nothing. */
void EftLinkMgr_Reset(EftVTask *task) {
}

/* Manager term. */
void EftLinkMgr_Term(EftVTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftLink);
    gEftLink = NULL;
}

/* Task init: copies the argument block, starts the grow and key animations, builds the sheet's UV table. */
void EftLink_Init(EftVTask *task, EftLinkArg *arg) {
    EftLinkWork *w = task->work;
    EftLinkDef *def = arg->def;
    s32 i;
    s32 j;
    s32 n;

    memset(w, 0, sizeof(EftLinkWork));
    w->arg = *arg;
    w->arg.tex = arg->tex;
    w->arg.def = arg->def;
    w->arg.def2 = arg->def2;
    EftLink_SelectTex(w, arg->tex, arg->frame, arg->unk3C);
    if (def->flags & 0x200) {
        EftLink_InitGrow(w);
        w->flags |= EFT_LINK_GROWING;
    } else {
        w->spacing = def->spacing[0];
        w->scale = 1.0f;
    }
    if (def->flags & 0x400) {
        EftLink_InitKeys(w);
        w->flags |= EFT_LINK_KEYED;
    } else {
        EftLink_SetKey(w, 2);
    }
    {
        f32 d[2] = { 0.0f, 0.0f };

        if (def->cols >= 2 || def->rows >= 2) {
            w->frames = def->cols * def->rows;
            d[0] = 1.0f / def->cols;
            d[1] = 1.0f / def->rows;
            n = 0;
            for (j = 0; j < def->rows; j++) {
                for (i = 0; i < def->cols; i++) {
                    w->uv[n][0] = d[0] * i;
                    w->uv[n][1] = d[1] * j;
                    w->uv[n][2] = d[0] * i + d[0];
                    w->uv[n][3] = d[1] * j + d[1];
                    n++;
                }
            }
            w->flags |= EFT_LINK_SHEET;
        }
    }
    if (def->flags & 2) {
        w->flags |= EFT_LINK_FLAG200;
    }
    w->flags |= EFT_LINK_ALIVE;
}

/* Task update: delay, fade out, grow and key animations, sprite placement and stepping, life, end of the task. */
void EftLink_Update(EftVTask *task) {
    EftLinkWork *w = task->work;
    EftLinkArg *arg = &w->arg;
    f32 fade = 1.0f;

    if (!BtlScene_IsEffectStopped(arg->objId, w->type)) {
        if (w->delay > 0.0f) {
            w->delay -= 1.0f;
        } else {
            if ((w->flags & EFT_LINK_FADING) && !(w->flags & EFT_LINK_STOPPING)) {
                f32 r = w->fade / w->fadeMax;

                if (r < 0.0f) {
                    fade = 0.0f;
                } else if (!(r > 1.0f)) {
                    fade = r;
                }
                w->fade -= 1.0f;
                if (w->fade < 0.0f) {
                    w->flags &= ~EFT_LINK_ALIVE;
                    w->flags |= EFT_LINK_KILLED;
                }
            }
            if (w->flags & EFT_LINK_GROWING) {
                EftLink_UpdateGrow(w);
            }
            if (w->flags & EFT_LINK_KEYED) {
                EftLink_UpdateKeys(w);
            }
            if (!(w->flags & EFT_LINK_STOPPED)) {
                EftLink_Place(w);
            }
            EftLink_StepNodes(w, fade);
            w->time += 1.0f;
            if (w->flags & EFT_LINK_KEYED) {
                EftLink_InitKeys(w);
                if (w->time >= w->keyDur) {
                    EftLink_SetKey(w, 2);
                    w->flags &= ~EFT_LINK_KEYED;
                }
            }
            if (arg->life * 30.0f <= w->time && arg->life > 0.0f) {
                if (w->fadeMax > 0.0f) {
                    w->flags |= 0x1000 | EFT_LINK_FADING;
                } else {
                    w->flags |= EFT_LINK_STOPPED;
                }
            }
            if (w->flags & EFT_LINK_STOPPING) {
                w->stopDelay -= 1.0f;
                if (w->stopDelay <= 0.0f) {
                    w->flags &= ~EFT_LINK_STOPPING;
                    if (!(w->flags & EFT_LINK_FADING)) {
                        w->flags |= EFT_LINK_STOPPED;
                    }
                }
            }
            if ((w->flags & EFT_LINK_STOPPED) && w->head == NULL) {
                w->flags &= ~EFT_LINK_ALIVE;
                w->flags |= EFT_LINK_DEAD;
            }
            if ((w->flags & EFT_LINK_KILLED) || (w->flags & EFT_LINK_DEAD)) {
                BtlTask_SetDead(task);
            }
        }
    }
    if (!(w->flags & EFT_LINK_KILLED)) {
        if (!(w->flags & EFT_LINK_DEAD)) {
            EftLink_BuildTex(w);
        }
    }
}

/* Task post-update: nothing. */
void EftLink_PostUpdate(EftVTask *task) {
}

/* The GS TEX0 value of the chain's texture. Integer arithmetic: the pointer form adds the other way round. */
#define EFT_LINK_TEX0(w) (*(u64 *)((u32)(w)->arg.tex + ((w)->arg.frame << 4)))

/* Task draw: draws every sprite of the chain with one of four routines chosen by the definition flags. */
void EftLink_Draw(EftVTask *task) {
    EftLinkWork *w = task->work;
    EftLinkDef *def = w->arg.def;
    Mtx44 m = { 0 };
    Vec4 pos = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec4 out = { 0.0f, 0.0f, 0.0f, 1.0f };
    EftLinkNode *n;
    f32 size;
    f32 rot;

    func_00120230(EFTV_MTX(&m), &gEftLink->identity);
    func_00120398(EFTV_MTX(&m), EFTV_MTX(&m), w->pitch);
    func_00120428(EFTV_MTX(&m), EFTV_MTX(&m), w->yaw);
    func_00120AB0();
    func_00120B80(&gBtlCamView->world2screen);
    for (n = w->head; n != NULL; n = n->next) {
        if (n->flags & 0x40) {
            if (def->flags & 0x10) {
                Vec4_Copy(EFTV_VEC(&pos), &n->offset);
                if (!(def->flags & 0x2000)) {
                    Vec3_Scale(EFTV_VEC(&pos), EFTV_VEC(&pos), w->arg.size * w->scale);
                }
                Mtx_MulVec4(EFTV_VEC(&pos), EFTV_MTX(&m), EFTV_VEC(&pos));
                Vec3_Add(EFTV_VEC(&out), EFTV_VEC(&pos), &n->pos);
                size = n->size * w->arg.size * w->scale * n->unkFC * 0.5f;
                rot = EftMath_WrapAngle(n->rot + n->twist);
                if (def->flags & 4) {
                    EftLink_DrawBillboardClipped(EFTV_VEC(&out), &n->color, &n->unk40, size, size, n->uv0.x, n->uv0.y, n->uv1.z, n->uv1.w, rot,
                                  def->layer, (w->flags >> 9) & 1, EFT_LINK_TEX0(w), 2.0f);
                } else {
                    EftLink_DrawBillboard(EFTV_VEC(&out), &n->color, (s32)n->unk40.x << 4, (s32)n->unk40.y << 4, size * 16.0f,
                                  size * 16.0f, n->uv0.x, n->uv0.y, n->uv1.z, n->uv1.w, rot, def->layer,
                                  (w->flags >> 9) & 1, EFT_LINK_TEX0(w));
                }
            } else if (def->flags & 4) {
                EftLink_DrawQuadClipped(n, &n->uv0, &n->uv1, &n->color, def->layer, w->arg.frame, (w->flags >> 9) & 1,
                              w->arg.tex);
            } else {
                EftLink_DrawQuad(n, &n->uv0, &n->uv1, &n->color, def->layer, w->arg.frame, (w->flags >> 9) & 1,
                              w->arg.tex);
            }
        }
    }
    func_00120AC8();
}

/* Task reset: ends the task. */
void EftLink_Reset(EftVTask *task) {
    BtlTask_SetDead(task);
}

/* Task term: returns the chain's sprites to the pool. */
void EftLink_Term(EftVTask *task) {
    EftLinkWork *w = task->work;
    EftLinkNode *n;

    for (n = w->head; n != NULL; n = n->next) {
        n->flags = 0;
        EftLink_UnlinkNode(&w->head, &w->tail, n);
    }
    w->flags = 0;
}

/* Lays the chain out from pos to pos2: one sprite every `spacing`, each pushed along the chain by a random
   amount; sprites that do not exist yet are taken from the pool. Also stores the chain's pitch and yaw. */
void EftLink_Place(EftLinkWork *w) {
    EftVVec *dir = &w->dir;
    EftLinkArg *arg = &w->arg;
    EftVVecU p = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    EftVVecU off = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    EftLinkNode *n;
    f32 len;
    f32 count = 0.0f;
    f32 a;
    f32 fi;
    s32 i;

    Vec3_Sub(dir, &w->arg.pos2, &w->arg.pos);
    len = Vec3_Length(dir);
    if (len > count) {
        count = len / w->spacing;
        Vec3_Normalize(dir, dir);
        a = (w->dir.y > 1.0f) ? -1.0f : (w->dir.y < -1.0f) ? 1.0f : -w->dir.y;
        w->pitch = EftMath_WrapAngle(asinf(a));
        w->yaw = EftMath_WrapAngle(atan2f(w->dir.x, w->dir.z));
        n = w->head;
        for (i = 0; i < (s32)count; i++) {
            fi = i;
            func_001225D0(EFTV_VEC(&p), &w->dir, &arg->pos, len * (fi / count));
            Vec3_Scale(EFTV_VEC(&off), &w->dir, Rand_FloatRange(w->val[11].cur, w->val[12].cur));
            Vec3_Add(EFTV_VEC(&p), EFTV_VEC(&p), EFTV_VEC(&off));
            if (n == NULL) {
                n = EftLink_NewNode(w, EFTV_VEC(&p), fi);
                if (n == NULL) {
                    continue;
                }
            }
            if (!(n->flags & 1)) {
                EftLink_InitNode(n, w);
                Vec4_Copy(&n->pos, EFTV_VEC(&p));
                n->twist = EftMath_WrapAngle(w->val[8].cur * 6.2831853f * fi);
                n->flags |= 1;
            }
            n->flags |= 0x400;
            n = n->next;
        }
    }
}

/* Starts the grow animation: three random scales and the per-frame steps of scale and spacing between them. */
void EftLink_InitGrow(EftLinkWork *w) {
    EftLinkDef *def = w->arg.def;
    f32 s[3];
    f32 dur;

    dur = def->growTime;
    w->growTime = 0.0f;
    w->growDur = dur * 30.0f;
    s[0] = Rand_FloatRange(def->scale[0], def->scale[0] + def->scaleRange[0]);
    s[1] = Rand_FloatRange(def->scale[1], def->scale[1] + def->scaleRange[1]);
    s[2] = Rand_FloatRange(def->scale[2], def->scale[2] + def->scaleRange[2]);
    w->scaleStep[0] = (s[1] - s[0]) / (w->growDur * def->growSplit);
    w->scaleStep[1] = (s[2] - s[1]) / (w->growDur * (1.0f - def->growSplit));
    w->scale = s[0];
    w->spacingStep[0] = (def->spacing[1] - def->spacing[0]) / (w->growDur * def->growSplit);
    w->spacingStep[1] = (def->spacing[2] - def->spacing[1]) / (w->growDur * (1.0f - def->growSplit));
    w->spacing = def->spacing[0];
}

/* Steps the grow animation; clears its flag when the time is up. */
void EftLink_UpdateGrow(EftLinkWork *w) {
    EftLinkDef *def = w->arg.def;

    if (w->growTime / w->growDur < def->growSplit) {
        w->scale += w->scaleStep[0];
        w->spacing += w->spacingStep[0];
    } else {
        w->scale += w->scaleStep[1];
        w->spacing += w->spacingStep[1];
    }
    w->growTime += 1.0f;
    if (w->growDur <= w->growTime) {
        w->flags &= ~EFT_LINK_GROWING;
    }
}

/* At the start of each key interval (time 0, then the split time): stores the difference between the interval's
   two keys for every animated value. */
/* NOT MATCHING: 282 instructions in the original, 402 here. Same operations; the original shares the addresses
   def + 4k, def + 4 + 4k, def + 8 + 4k, def + 12 + 4k between the statements (it adds the 16-byte multiple of
   each field offset as a displacement) and spills them, this C recomputes them. */
#if 0
void EftLink_InitKeys(EftLinkWork *w) {
    EftLinkDef *def = w->arg.def;
    EftLinkDef2 *def2 = w->arg.def2;
    s32 k = 0;

    if (w->time <= 0.0f) {
        k = 1;
        w->keyDur = def->keyTime * 30.0f;
        w->keySplit = w->keyDur * def->keySplit;
    }
    if (w->keySplit <= w->time && !(w->flags & EFT_LINK_KEY2)) {
        k = 2;
        w->flags |= EFT_LINK_KEY2;
    }
    if (k != 0) {
        w->val[0].delta = def->val[0][k] - def->val[0][k - 1];
        w->val[1].delta = def->val[1][k] - def->val[1][k - 1];
        w->val[2].delta = def->val[2][k] - def->val[2][k - 1];
        w->val[3].delta = def->val[3][k] - def->val[3][k - 1];
        w->val[4].delta = def->val[4][k] - def->val[4][k - 1];
        w->val[5].delta = def->val[5][k] - def->val[5][k - 1];
        w->val[6].delta = def->val[6][k] - def->val[6][k - 1];
        w->val[7].delta = def->val[7][k] - def->val[7][k - 1];
        w->val[8].delta = def->val[8][k] - def->val[8][k - 1];
        w->val[9].delta = def->val[9][k] - def->val[9][k - 1];
        w->val[10].delta = def->val[10][k] - def->val[10][k - 1];
        w->val[11].delta = def->val[11][k] - def->val[11][k - 1];
        w->val[12].delta = def->val[12][k] - def->val[12][k - 1];
        w->val[13].delta = def->val[13][k] - def->val[13][k - 1];
        w->val[14].delta = def->val[14][k] - def->val[14][k - 1];
        w->val[15].delta = def->val[15][k] - def->val[15][k - 1];
        w->vecA[1][0] = def->vecA[k][0] - def->vecA[k - 1][0];
        w->vecA[1][1] = def->vecA[k][1] - def->vecA[k - 1][1];
        w->vecA[1][2] = def->vecA[k][2] - def->vecA[k - 1][2];
        w->vecB[1][0] = def->vecB[k][0] - def->vecB[k - 1][0];
        w->vecB[1][1] = def->vecB[k][1] - def->vecB[k - 1][1];
        w->vecB[1][2] = def->vecB[k][2] - def->vecB[k - 1][2];
        w->val2[0].delta = def->val2[0][k] - def->val2[0][k - 1];
        w->val2[1].delta = def->val2[1][k] - def->val2[1][k - 1];
        w->val2[2].delta = def->val2[2][k] - def->val2[2][k - 1];
        w->val2[3].delta = def->val2[3][k] - def->val2[3][k - 1];
        w->pair2[1].a = def->pair[k].a - def->pair[k - 1].a;
        w->pair2[1].b = def->pair[k].b - def->pair[k - 1].b;
        w->val3.delta = def->val3[k] - def->val3[k - 1];
        w->pair[0][1].a = def2->pair[0][k].a - def2->pair[0][k - 1].a;
        w->pair[0][1].b = def2->pair[0][k].b - def2->pair[0][k - 1].b;
        w->pair[1][1].a = def2->pair[1][k].a - def2->pair[1][k - 1].a;
        w->pair[1][1].b = def2->pair[1][k].b - def2->pair[1][k - 1].b;
        w->pair[2][1].a = def2->pair[2][k].a - def2->pair[2][k - 1].a;
        w->pair[2][1].b = def2->pair[2][k].b - def2->pair[2][k - 1].b;
        w->val4[0].delta = def2->val[0][k] - def2->val[0][k - 1];
        w->val4[1].delta = def2->val[1][k] - def2->val[1][k - 1];
        w->val4[2].delta = def2->val[2][k] - def2->val[2][k - 1];
        Vec4_Sub(&w->vec[0][1], &def2->vec[0][k], &def2->vec[0][k - 1]);
        Vec4_Sub(&w->vec[2][1], &def2->vec[2][k], &def2->vec[2][k - 1]);
        Vec4_Sub(&w->vec[1][1], &def2->vec[1][k], &def2->vec[1][k - 1]);
        Vec4_Sub(&w->vec[3][1], &def2->vec[3][k], &def2->vec[3][k - 1]);
    }
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/eft_v_c", EftLink_InitKeys);

/* Every frame of the key animation: current value = first key of the interval + difference * progress. */
/* NOT MATCHING: same length (354 instructions), 225 differ: the same address sharing as in EftLink_InitKeys,
   which changes register allocation and the stack slots of the spilled addresses. */
#if 0
void EftLink_UpdateKeys(EftLinkWork *w) {
    EftLinkDef *def = w->arg.def;
    EftLinkDef2 *def2 = w->arg.def2;
    EftVVec tmp = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    s32 k;
    f32 t;

    if (!(w->flags & EFT_LINK_KEY2)) {
        k = 0;
        t = w->time / w->keySplit;
    } else {
        k = 1;
        t = (w->time - w->keySplit) / (w->keyDur - w->keySplit);
    }
    w->val[0].cur = def->val[0][k] + w->val[0].delta * t;
    w->val[1].cur = def->val[1][k] + w->val[1].delta * t;
    w->val[2].cur = def->val[2][k] + w->val[2].delta * t;
    w->val[3].cur = def->val[3][k] + w->val[3].delta * t;
    w->val[4].cur = def->val[4][k] + w->val[4].delta * t;
    w->val[5].cur = def->val[5][k] + w->val[5].delta * t;
    w->val[6].cur = def->val[6][k] + w->val[6].delta * t;
    w->val[7].cur = def->val[7][k] + w->val[7].delta * t;
    w->val[8].cur = def->val[8][k] + w->val[8].delta * t;
    w->val[9].cur = def->val[9][k] + w->val[9].delta * t;
    w->val[10].cur = def->val[10][k] + w->val[10].delta * t;
    w->val[11].cur = def->val[11][k] + w->val[11].delta * t;
    w->val[12].cur = def->val[12][k] + w->val[12].delta * t;
    w->val[13].cur = def->val[13][k] + w->val[13].delta * t;
    w->val[14].cur = def->val[14][k] + w->val[14].delta * t;
    w->val[15].cur = def->val[15][k] + w->val[15].delta * t;
    w->vecA[0][0] = def->vecA[k][0] + w->vecA[1][0] * t;
    w->vecA[0][1] = def->vecA[k][1] + w->vecA[1][1] * t;
    w->vecA[0][2] = def->vecA[k][2] + w->vecA[1][2] * t;
    w->vecB[0][0] = def->vecB[k][0] + w->vecB[1][0] * t;
    w->vecB[0][1] = def->vecB[k][1] + w->vecB[1][1] * t;
    w->vecB[0][2] = def->vecB[k][2] + w->vecB[1][2] * t;
    w->val2[0].cur = def->val2[0][k] + w->val2[0].delta * t;
    w->val2[1].cur = def->val2[1][k] + w->val2[1].delta * t;
    w->val2[2].cur = def->val2[2][k] + w->val2[2].delta * t;
    w->val2[3].cur = def->val2[3][k] + w->val2[3].delta * t;
    w->pair2[0].a = def->pair[k].a + w->pair2[1].a * t;
    w->pair2[0].b = def->pair[k].b + w->pair2[1].b * t;
    w->val3.cur = def->val3[k] + w->val3.delta * t;
    w->pair[0][0].a = def2->pair[0][k].a + w->pair[0][1].a * t;
    w->pair[0][0].b = def2->pair[0][k].b + w->pair[0][1].b * t;
    w->pair[1][0].a = def2->pair[1][k].a + w->pair[1][1].a * t;
    w->pair[1][0].b = def2->pair[1][k].b + w->pair[1][1].b * t;
    w->pair[2][0].a = def2->pair[2][k].a + w->pair[2][1].a * t;
    w->pair[2][0].b = def2->pair[2][k].b + w->pair[2][1].b * t;
    w->val4[0].cur = def2->val[0][k] + w->val4[0].delta * t;
    w->val4[1].cur = def2->val[1][k] + w->val4[1].delta * t;
    w->val4[2].cur = def2->val[2][k] + w->val4[2].delta * t;
    Vec4_Scale(&tmp, &w->vec[0][1], t);
    Vec4_Add(&w->vec[0][0], &def2->vec[0][k], &tmp);
    Vec4_Scale(&tmp, &w->vec[2][1], t);
    Vec4_Add(&w->vec[2][0], &def2->vec[2][k], &tmp);
    Vec4_Scale(&tmp, &w->vec[1][1], t);
    Vec4_Add(&w->vec[1][0], &def2->vec[1][k], &tmp);
    Vec4_Scale(&tmp, &w->vec[3][1], t);
    Vec4_Add(&w->vec[3][0], &def2->vec[3][k], &tmp);
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/eft_v_c", EftLink_UpdateKeys);
