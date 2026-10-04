#include "common.h"
#include "battle/eft_ad.h"

/*
 * 0x1A9D90..0x1AA7E8: two camera-facing sprites written straight into the order table. This is the head of the
 * next source file (it goes on at 0x1AA7E8 with the scalar screen test both functions call); the object before it
 * ends with the out-of-line copy of an inline function at 0x1A9D38.
 * Both functions are still assembly: the attempts below were read from the disassembly and compile, but were not
 * brought to a match.
 */

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftAdCView {
    /* 0x000 */ u8 unk0[0x140];
    /* 0x140 */ Mtx44 world2screen;
    /* 0x180 */ u8 unk180[0xB4];
    /* 0x234 */ f32 sprScale;   /* projection scale used for sprite sizes */
} EftAdCView;

typedef struct EftAdCOtPrim {
    u32 tag;
    void *next;
} EftAdCOtPrim;

typedef struct EftAdCOtEntry {
    EftAdCOtPrim *head;
    EftAdCOtPrim *tail;
} EftAdCOtEntry;

typedef struct EftAdCOtSlot {
    EftAdCOtEntry layer[2];
} EftAdCOtSlot;

typedef struct EftAdCXyzf {
    u64 x : 16;
    u64 y : 16;
    u64 z : 24;
    u64 f : 8;
} EftAdCXyzf;

/* Textured quad as a triangle strip with one colour. */
typedef struct EftSprPkt {
    /* 0x00 */ u32 dmaTag;     /* 0x20000007 */
    /* 0x04 */ void *next;
    /* 0x08 */ u32 vif0;       /* 0x10000000 */
    /* 0x0C */ u32 vif1;       /* 0x50000007 */
    /* 0x10 */ u64 gifTag;     /* 0xC400000000008001: REGLIST, 12 registers */
    /* 0x18 */ u64 regs;       /* PRIM, TEX0_1 (TEX0_2 with ctx), RGBAQ, 4 x (ST, XYZF2), NOP */
    /* 0x20 */ u64 prim;       /* 0x54 (textured blended triangle strip) | ctx << 9 */
    /* 0x28 */ u64 tex0;
    /* 0x30 */ u8 r, g, b, a;
    /* 0x34 */ f32 q;          /* 1.0 */
    /* 0x38 */ struct {
        f32 s, t;
        EftAdCXyzf xyz;
    } v[4];
    /* 0x78 */ u64 nop;
} EftSprPkt; /* 0x80 */

extern EftAdCView *gBtlCamView;
extern u8 *gOtCur;
extern EftAdCOtSlot *gOtZ;
extern EftAdVec gEftSprUvScale; /* {1, 1.1666666, 1, 1}: the screen's pixel aspect, applied to the rotated corners */

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void func_00120308(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Z */
extern void func_00121F08(Vec4 *dst, Vec4 *a, Vec4 *b);       /* per-component product */
extern void func_001220F0(EftAdScr *dst, Vec4 *src);          /* float vector to integer vector */
extern s32 func_00122350(EftAdScr *out, Mtx44 *m, Vec4 *pos); /* projects one point; out->w = view depth */
extern s32 EftSpr_IsOffScreen(s32 x, s32 y, s32 z);                /* 1 = outside the GS drawing area */

/* Queues an upright camera-facing sprite at pos: w x h (half sizes, in screen units at the reference depth,
   scaled by 4096 * view scale / depth), one colour, uv rectangle (u0, v0)-(u1, v1). Dropped when smaller than
   two units or when any corner is off screen. */
/* Not matching: 232 of 250 instructions differ (the original is 254); not iterated. */
#if 0
void EftSpr_DrawFlat(u8 r, u8 g, u8 b, u8 a, f32 u0, f32 v0, f32 u1, f32 v1, Vec4 *pos, u32 w, u32 h, s8 ctx,
                     s32 layer, u64 *tex0) {
    EftAdScr scr;
    EftSprPkt *p;
    EftAdCOtEntry *e;
    f32 scale = gBtlCamView->sprScale;
    u32 k;
    s32 z;
    s32 l;

    if (w == 0 || h == 0) {
        return;
    }
    func_00122350(&scr, &gBtlCamView->world2screen, pos);
    k = scale * 4096.0f / scr.w;
    w = (w * k) >> 12;
    h = (h * k) >> 12;
    if (w < 2 || h < 2) {
        return;
    }
    if (EftSpr_IsOffScreen(scr.x - w, scr.y - h, scr.z)) {
        return;
    }
    if (EftSpr_IsOffScreen(scr.x - w, scr.y + h, scr.z)) {
        return;
    }
    if (EftSpr_IsOffScreen(scr.x + w, scr.y + h, scr.z)) {
        return;
    }
    if (EftSpr_IsOffScreen(scr.x + w, scr.y - h, scr.z)) {
        return;
    }
    p = (EftSprPkt *)gOtCur;
    gOtCur = (u8 *)(p + 1);
    p->prim = ((s64)ctx << 9) | 0x40 | 0x14;
    p->dmaTag = 0x20000007;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000007;
    p->gifTag = 0xC400000000008001;
    p->regs = (ctx << 4) + 0xF42424242160;
    p->next = NULL;
    l = layer;
    if (l >= 2) {
        l -= 2;
    }
    p->tex0 = *tex0;
    p->r = r;
    p->g = g;
    p->b = b;
    p->a = a;
    p->q = 1.0f;
    p->v[1].s = u0;
    p->v[2].t = v0;
    p->v[3].s = u1;
    p->v[3].t = v1;
    p->v[0].s = u0;
    p->v[0].t = v0;
    p->v[1].t = v1;
    p->v[2].s = u1;
    p->v[0].xyz.x = scr.x - w;
    p->v[0].xyz.y = scr.y - h;
    p->v[0].xyz.z = scr.z;
    p->v[0].xyz.f = 0xFF;
    p->v[1].xyz.x = scr.x + w;
    p->v[1].xyz.y = scr.y - h;
    p->v[1].xyz.z = scr.z;
    p->v[1].xyz.f = 0xFF;
    p->v[2].xyz.x = scr.x - w;
    p->v[2].xyz.y = scr.y + h;
    p->v[2].xyz.z = scr.z;
    p->v[2].xyz.f = 0xFF;
    p->v[3].xyz.x = scr.x + w;
    p->v[3].xyz.y = scr.y + h;
    p->v[3].xyz.z = scr.z;
    p->v[3].xyz.f = 0xFF;
    z = scr.z >> 8;
    if (z < 0) {
        e = &gOtZ[0].layer[l];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[l];
    } else {
        e = &gOtZ[z].layer[l];
    }
    e->tail->next = p;
    e->tail = (EftAdCOtPrim *)p;
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/eft_ad_c", EftSpr_DrawFlat);

/* Queues a camera-facing sprite rotated by rot radians in the screen plane, centred on (x, y, z) plus an offset
   (ofsX, ofsY) in sprite units. Sizes are scaled by size * view scale / depth; the rotated corners are multiplied
   by gEftSprUvScale (y x 7/6, the pixel aspect). Dropped when smaller than two units or when any corner is off
   screen. `unused` is not read. */
/* Not matching: 325 instructions against 407: the original has the four corners written out one by one (four
   screen tests and four vertex blocks) where this C uses loops; not iterated. Its float constants compile to the
   original's bits (pi, 2 pi, -pi, 2 pi at 0x2FCEF4..0x2FCF04). */
#if 0
void EftSpr_DrawRot(u8 r, u8 g, u8 b, u8 a, f32 x, f32 y, f32 z, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot, s32 ofsX,
                    s32 ofsY, u32 w, u32 h, s32 unused, u32 size, s8 ctx, s32 layer, u64 *tex0) {
    EftAdScr scr;
    EftAdScr c[4];
    EftAdVec pos;
    EftAdVec f[4];
    Mtx44 m;
    EftSprPkt *p;
    EftAdCOtEntry *e;
    f32 scale = gBtlCamView->sprScale;
    u32 k;
    s32 ox;
    s32 oy;
    s32 i;
    s32 sz;
    s32 l;

    Vec4_Set((Vec4 *)&pos, x, y, z, 1.0f);
    func_00122350(&scr, &gBtlCamView->world2screen, (Vec4 *)&pos);
    k = size * scale / scr.w;
    w = (w * k) >> 12;
    h = (h * k) >> 12;
    if (w < 2 || h < 2) {
        return;
    }
    if (rot > 3.14159265f) {
        rot -= 6.2831853f;
    } else if (rot < -3.14159265f) {
        rot += 6.2831853f;
    }
    Mtx_StoreIdentity(&m);
    func_00120308(&m, &m, rot);
    if (ofsX >= 0) {
        ox = (s32)(ofsX * k) >> 12;
    } else {
        ox = -((s32)(-ofsX * k) >> 12);
    }
    if (ofsY >= 0) {
        oy = (s32)(ofsY * k) >> 12;
    } else {
        oy = -((s32)(-ofsY * k) >> 12);
    }
    c[0].x = ox - w;
    c[0].y = oy - h;
    c[0].z = 0;
    c[0].w = 1;
    c[1].x = ox - w;
    c[1].y = oy + h;
    c[1].z = 0;
    c[1].w = 1;
    c[2].x = ox + w;
    c[2].y = oy - h;
    c[2].z = 0;
    c[2].w = 1;
    c[3].x = ox + w;
    c[3].y = oy + h;
    c[3].z = 0;
    c[3].w = 1;
    for (i = 0; i < 4; i++) {
        f[i].x = c[i].x;
        f[i].y = c[i].y;
        f[i].z = c[i].z;
        f[i].w = c[i].w;
        Mtx_MulVec4((Vec4 *)&f[i], &m, (Vec4 *)&f[i]);
        func_00121F08((Vec4 *)&f[i], (Vec4 *)&f[i], (Vec4 *)&gEftSprUvScale);
        func_001220F0(&c[i], (Vec4 *)&f[i]);
    }
    for (i = 0; i < 4; i++) {
        if (EftSpr_IsOffScreen(scr.x + c[i].x, scr.y + c[i].y, scr.z)) {
            return;
        }
    }
    p = (EftSprPkt *)gOtCur;
    gOtCur = (u8 *)(p + 1);
    p->prim = ((s64)ctx << 9) | 0x40 | 0x14;
    p->dmaTag = 0x20000007;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000007;
    p->gifTag = 0xC400000000008001;
    p->regs = (ctx << 4) + 0xF42424242160;
    p->next = NULL;
    l = layer;
    if (l >= 2) {
        l -= 2;
    }
    p->tex0 = *tex0;
    p->r = r;
    p->g = g;
    p->b = b;
    p->a = a;
    p->q = 1.0f;
    p->v[1].s = u0;
    p->v[2].t = v0;
    p->v[3].s = u1;
    p->v[3].t = v1;
    p->v[0].s = u0;
    p->v[0].t = v0;
    p->v[1].t = v1;
    p->v[2].s = u1;
    for (i = 0; i < 4; i++) {
        p->v[i].xyz.x = scr.x + c[i].x;
        p->v[i].xyz.y = scr.y + c[i].y;
        p->v[i].xyz.z = scr.z;
        p->v[i].xyz.f = 0xFF;
    }
    sz = scr.z >> 8;
    if (sz < 0) {
        e = &gOtZ[0].layer[l];
    } else if (sz >= 0x1000) {
        e = &gOtZ[0xFFF].layer[l];
    } else {
        e = &gOtZ[sz].layer[l];
    }
    e->tail->next = p;
    e->tail = (EftAdCOtPrim *)p;
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/eft_ad_c", EftSpr_DrawRot);
