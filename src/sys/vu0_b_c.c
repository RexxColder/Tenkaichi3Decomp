#include "common.h"
#include "sys/math3d.h"
#include "sys/mathf.h"
#include "sys/randf.h"

/*
 * The compiled-C functions of the vector / matrix library's second half (0x121008..0x122940).
 *
 * The range is mostly hand-written VU0 assembly, which stays assembly in the matching build; src/port/vu0_b.c has the
 * exact portable equivalent of every routine. The functions here are the ones the compiler produced. They are in
 * address order but NOT contiguous: the assembly routines between them are named in the comments marked "gap".
 */

/* The polygon clipper's vertex: position, texture coordinate, colour. */
typedef struct ClipVtx {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 uv;
    /* 0x20 */ Vec4 col;
} ClipVtx; /* size 0x30 */

extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern s32 memcmp(const void *a, const void *b, u32 n);
extern void Rand_Init(void);

/* First half of the library (config/symbols/vu0_a.txt). */
extern void Vu0_InitAxisRegs(void);                /* vf1-3 = unit vectors */
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_RotateZXY(Mtx44 *out, const Mtx44 *m, Vec4 *angles); /* out = m rotated by Euler angles */
extern void Vu0Cur_Init(void);                /* current matrix = identity, stack pointer = 0 */
extern void Vu0Cur_LoadIdentity(void);                /* current matrix = identity */
extern void Vu0Cur_Push(void);                /* push the current matrix */
extern void Vu0Cur_Pop(void);                /* pop the current matrix */
extern void Vu0Cur_ResetStack(void);                /* stack pointer = 0 */
extern s32 Vu0Cur_IsStackUsed(void);                 /* stack depth != 0 */
extern void Vu0Cur_LoadMtx(Mtx44 *m);            /* current matrix = m */
extern void Vu0Cur_RotateZXY(Vec4 *angles);        /* rotates the current matrix by Euler angles */
extern void Vu0Cur_MulVec4(void *out, void *v);  /* out = current matrix * v, four components */
extern void Vu0Cur_MulVec3(void *out, void *v);  /* the same, x, y, z only */

/* Assembly routines of this half. */
extern void Vu0View_LoadMtx(Mtx44 *m);
extern void Vu0View_StoreMtx(Mtx44 *m);
extern void ClipVtx_Copy(ClipVtx *dst, ClipVtx *src);
extern void ClipVtx_CopyArray(ClipVtx *dst, ClipVtx *src, s32 n);
extern void ClipVtx_Lerp(ClipVtx *out, ClipVtx *a, ClipVtx *b, f32 t);
extern f32 ClipPlane_EdgeParam(Vec4 *plane, ClipVtx *a, ClipVtx *b, f32 dist);
extern void ClipPlane_DistArray(Vec4 *dist, ClipVtx *poly, Vec4 *plane, s32 n);
extern void Mtx_MulVec3(void *out, Mtx44 *m, Vec4 *v);
extern void Mtx_MulVec4(Vec4 *out, Mtx44 *m, Vec4 *v);
extern f32 Vec3_Length(Vec4 *v);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Cross(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Add(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *out, Vec4 *v, f32 s);

extern const Mtx44 D_002EC260; /* the identity matrix */
extern f32 D_002FC32C;         /* 0.1234141f, the boot seed */

void Vec3_Set(Vec4 *v, f32 x, f32 y, f32 z);
void Vec3_DirToEuler(Vec4 *out, Vec4 *dir);

/* 0x121008: out = (identity rotated by the Euler angles) * v, through the current matrix, which is not restored. */
void Vu0Cur_RotEulerMulVec4(Vec4 *out, Vec4 *angles, Vec4 *v) {
    Vu0Cur_LoadIdentity();
    Vu0Cur_RotateZXY(angles);
    Vu0Cur_MulVec4(out, v);
}

/* 0x121058: the same, writing x, y, z only. */
void Vu0Cur_RotEulerMulVec3(Vec4 *out, Vec4 *angles, Vec4 *v) {
    Vu0Cur_LoadIdentity();
    Vu0Cur_RotateZXY(angles);
    Vu0Cur_MulVec3(out, v);
}

/* gap 0x1210A8..0x1214A0: Vu0Cur_Project*, Vu0Clip_*, Vu0Screen_*, Vu0View_*Mtx (VU0 assembly). */

/* 0x1214A0: vf28-31 = rotation X, Z, Y (libm sinf / cosf) with row 3 = rotation * trans. */
void Vu0View_SetRotTrans(Vec4 *rot, Vec4 *trans) {
    Mtx44 m;
    Mtx44 r;
    f32 s;
    f32 c;

    Vu0Cur_Push();
    Vu0View_StoreMtx(&m);
    Mtx_StoreIdentity(&m);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->x);
    c = cosf(rot->x);
    r.m[1][1] = c;
    r.m[1][2] = -s;
    r.m[2][1] = s;
    r.m[2][2] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[1], r.m[1]);
    Vu0Cur_MulVec4(m.m[2], r.m[2]);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->z);
    c = cosf(rot->z);
    r.m[0][0] = c;
    r.m[0][1] = -s;
    r.m[1][0] = s;
    r.m[1][1] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[0], r.m[0]);
    Vu0Cur_MulVec4(m.m[1], r.m[1]);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->y);
    c = cosf(rot->y);
    r.m[0][0] = c;
    r.m[0][2] = s;
    r.m[2][0] = -s;
    r.m[2][2] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[0], r.m[0]);
    Vu0Cur_MulVec4(m.m[2], r.m[2]);
    Mtx_MulVec3(m.m[3], &m, trans);
    Vu0View_LoadMtx(&m);
    Vu0Cur_Pop();
}

/* 0x121640: vf28-31: row 3 = M * trans, then rotations Y, Z, X applied. No callers. */
void Vu0View_ApplyTransRot(Vec4 *rot, Vec4 *trans) {
    Mtx44 m;
    Mtx44 r;
    f32 s;
    f32 c;

    Vu0Cur_Push();
    Vu0View_StoreMtx(&m);
    Mtx_MulVec3(m.m[3], &m, trans);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->y);
    c = cosf(rot->y);
    r.m[0][0] = c;
    r.m[0][2] = s;
    r.m[2][0] = -s;
    r.m[2][2] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[0], r.m[0]);
    Vu0Cur_MulVec4(m.m[2], r.m[2]);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->z);
    c = cosf(rot->z);
    r.m[0][0] = c;
    r.m[0][1] = -s;
    r.m[1][0] = s;
    r.m[1][1] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[0], r.m[0]);
    Vu0Cur_MulVec4(m.m[1], r.m[1]);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->x);
    c = cosf(rot->x);
    r.m[1][1] = c;
    r.m[1][2] = -s;
    r.m[2][1] = s;
    r.m[2][2] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[1], r.m[1]);
    Vu0Cur_MulVec4(m.m[2], r.m[2]);
    Vu0View_LoadMtx(&m);
    Vu0Cur_Pop();
}

/*
 * Two rows of a rotation matrix through the current matrix (vf16-19): ra = cur * a, rb = cur * b, then the two results
 * replace two rows of the current matrix.
 */
#define VU0_CUR_MUL2(a, b, ra, rb, da, db) \
    __asm__ volatile( \
        "lqc2 $vf8, %0\n" \
        "lqc2 $vf9, %1\n" \
        "vmulax.xyzw $ACC, $vf16, $vf8x\n" \
        "vmadday.xyzw $ACC, $vf17, $vf8y\n" \
        "vmaddaz.xyzw $ACC, $vf18, $vf8z\n" \
        "vmaddw.xyzw " ra ", $vf19, $vf8w\n" \
        "vmulax.xyzw $ACC, $vf16, $vf9x\n" \
        "vmadday.xyzw $ACC, $vf17, $vf9y\n" \
        "vmaddaz.xyzw $ACC, $vf18, $vf9z\n" \
        "vmaddw.xyzw " rb ", $vf19, $vf9w\n" \
        "vmove.xyzw " da ", $vf7\n" \
        "vmove.xyzw " db ", $vf4\n" \
        : : "m"(a), "m"(b))

/*
 * 0x1217D0: current matrix = current * Ry(rot.y) * Rx(rot.x) * Rz(rot.z), each rotation applied to the two rows it
 * changes; libm cosf / sinf. Compiled C with inline VU0 assembly.
 */
void Vu0Cur_RotateYXZ(Vec4 *rot) {
    Mtx44 m;

    Mtx_StoreIdentity(&m);
    m.m[0][0] = cosf(rot->y);
    m.m[2][0] = sinf(rot->y);
    m.m[2][2] = m.m[0][0];
    m.m[0][2] = -m.m[2][0];
    VU0_CUR_MUL2(m.m[0][0], m.m[2][0], "$vf7", "$vf4", "$vf16", "$vf18");
    Mtx_StoreIdentity(&m);
    m.m[1][1] = cosf(rot->x);
    m.m[2][1] = sinf(rot->x);
    m.m[2][2] = m.m[1][1];
    m.m[1][2] = -m.m[2][1];
    VU0_CUR_MUL2(m.m[1][0], m.m[2][0], "$vf7", "$vf4", "$vf17", "$vf18");
    Mtx_StoreIdentity(&m);
    m.m[0][0] = cosf(rot->z);
    m.m[1][0] = sinf(rot->z);
    m.m[1][1] = m.m[0][0];
    m.m[0][1] = -m.m[1][0];
    __asm__ volatile(
        "lqc2 $vf8, %0\n"
        "lqc2 $vf9, %1\n"
        "vmulax.xyzw $ACC, $vf16, $vf8x\n"
        "vmadday.xyzw $ACC, $vf17, $vf8y\n"
        "vmaddaz.xyzw $ACC, $vf18, $vf8z\n"
        "vmaddw.xyzw $vf4, $vf19, $vf8w\n"
        "vmulax.xyzw $ACC, $vf16, $vf9x\n"
        "vmadday.xyzw $ACC, $vf17, $vf9y\n"
        "vmaddaz.xyzw $ACC, $vf18, $vf9z\n"
        "vmaddw.xyzw $vf7, $vf19, $vf9w\n"
        "vmove.xyzw $vf16, $vf4\n"
        "vmove.xyzw $vf17, $vf7\n"
        : : "m"(m.m[0][0]), "m"(m.m[1][0]));
}

/* 0x121910..0x121950: eight empty functions, no callers. */
void Vu0_Stub0(void) {
}

void Vu0_Stub1(void) {
}

void Vu0_Stub2(void) {
}

void Vu0_Stub3(void) {
}

void Vu0_Stub4(void) {
}

void Vu0_Stub5(void) {
}

void Vu0_Stub6(void) {
}

void Vu0_Stub7(void) {
}

/* gap 0x121950..0x121A10: ClipVtx_Set, ClipVtx_Copy, ClipVtx_SetArray, ClipVtx_CopyArray (assembly). */

/*
 * 0x121A10: clips a convex polygon of n vertices (at most 9 in, 9 out) against one plane, in place, and returns the
 * new vertex count. A vertex is outside when dot(plane.xyz, pos) + plane.w < 0. Each edge that crosses the plane gets
 * a vertex at t = dist(cur) / dot(plane, cur - next), interpolated as next * t + cur * (1 - t) on all three quadwords.
 */
s32 ClipPoly_ClipPlane(ClipVtx *poly, Vec4 *plane, s32 n) {
    ClipVtx buf[9];
    Vec4 dist[9];
    s32 out[12];
    ClipVtx *dst;
    ClipVtx *cur;
    ClipVtx *next;
    s32 i;
    s32 j;
    s32 nOut;
    u32 cnt;
    f32 t;

    if (n == 0) {
        return 0;
    }
    ClipPlane_DistArray(dist, poly, plane, n);
    nOut = 0;
    for (i = 0; i < n; i++) {
        out[i] = dist[i].x < 0.0f;
        nOut += out[i];
    }
    if (nOut == 0) {
        return n;
    }
    if (n == nOut) {
        return 0;
    }
    dst = buf;
    j = 1;
    for (i = 0; i < n; i++, j++) {
        cur = &poly[i];
        if (j >= n) {
            j = 0;
        }
        next = &poly[j];
        if (out[i] == 0) {
            ClipVtx_Copy(dst, cur);
            dst++;
            if (out[j] != 0) {
                t = ClipPlane_EdgeParam(plane, cur, next, dist[i].x);
                ClipVtx_Lerp(dst, next, cur, t);
                dst++;
            }
        } else if (out[j] == 0) {
            t = ClipPlane_EdgeParam(plane, cur, next, dist[i].x);
            ClipVtx_Lerp(dst, next, cur, t);
            dst++;
        }
    }
    cnt = (u32)((u8 *)dst - (u8 *)buf) / sizeof(ClipVtx);
    ClipVtx_CopyArray(poly, buf, cnt);
    return cnt;
}

/* gap 0x121C18..0x121DA8: ClipVtx_Lerp, ClipPlane_EdgeParam, ClipPlane_DistArray, ClipPoly_ProjectMtx / Cur (assembly). */

/* 0x121DA8: boot-time set-up of the VU0 state and of both random generators. */
void Vu0_Init(void) {
    Rand_SeedFloat(D_002FC32C);
    Vu0_InitAxisRegs();
    Vu0Cur_Init();
    Vu0Cur_ResetStack();
    Rand_Init();
}

/* 0x121DE0: the remains of an assert: compares a stored identity with the constant one and reads the stack depth. */
void Vu0_CheckState(void) {
    Mtx44 m;

    Mtx_StoreIdentity(&m);
    memcmp(&m, &D_002EC260, sizeof(Mtx44));
    Vu0Cur_IsStackUsed();
}

/* gap 0x121E18..0x121E28: Vec4_SetZeroW1, Vec4_SetZero (assembly). */

/* 0x121E28: v = (x, y, z, w). */
void Vec4_Set(Vec4 *v, f32 x, f32 y, f32 z, f32 w) {
    v->x = x;
    v->y = y;
    v->z = z;
    v->w = w;
}

/* 0x121E40: sets x, y, z and leaves w. */
void Vec3_Set(Vec4 *v, f32 x, f32 y, f32 z) {
    v->x = x;
    v->y = y;
    v->z = z;
}

/* gap 0x121E50..0x122030: Vec3_Normalize .. Mtx_MulVec3 (assembly). */

/* 0x122030: out = (identity rotated by the Euler angles) * v, all four components. */
void Vec4_RotateEuler(Vec4 *out, Vec4 *angles, Vec4 *v) {
    Mtx44 m;

    Mtx_RotateZXY(&m, &D_002EC260, angles);
    Mtx_MulVec4(out, &m, v);
}

/* gap 0x122088..0x122258: Vec3_Dot .. Vec3_DistSq (assembly). */

/* 0x122258: yaw and pitch of a direction: out = (-atan2f(y, length(x, 0, z)), atan2f(x, z), 0); out.w is left. */
void Vec3_DirToEuler(Vec4 *out, Vec4 *dir) {
    Vec4 flat;

    out->y = atan2f(dir->x, dir->z);
    Vec3_Set(&flat, dir->x, 0.0f, dir->z);
    out->x = -atan2f(dir->y, Vec3_Length(&flat));
    out->z = 0.0f;
}

/* 0x1222D8: Vec3_DirToEuler of a - b. No callers. */
void Vec3_DiffToEuler(Vec4 *out, Vec4 *a, Vec4 *b) {
    Vec4 d;

    Vec3_Sub(&d, a, b);
    Vec3_DirToEuler(out, &d);
}

/* gap 0x122310..0x122588: Mtx_Project* (assembly). */

/* 0x122588: empty, no callers. */
void Vu0_Stub8(void) {
}

/* gap 0x122590..0x122698: Vec3_AddSub .. Vec3_AddClamp (assembly). */

/*
 * 0x122698: rotates v about a unit axis (Rodrigues): v cos + (axis x v) sin + axis (axis . v)(1 - cos). The three
 * terms are full four-component vectors; w is then replaced by v->w, read AFTER out was written, so with out == v the
 * result's w is the sum's w.
 */
void Vec3_RotateAxis(Vec4 *out, Vec4 *v, Vec4 *axis, f32 angle) {
    Vec4 a;
    Vec4 b;
    Vec4 c;
    f32 cs;
    f32 sn;

    cs = Mathf_Cos(angle);
    sn = Mathf_Sin(angle);
    Vec4_Scale(&a, v, cs);
    Vec3_Cross(&b, axis, v);
    Vec4_Scale(&b, &b, sn);
    Vec4_Scale(&c, axis, Vec3_Dot(axis, v) * (1.0f - cs));
    Vec4_Add(out, &a, &b);
    Vec4_Add(out, out, &c);
    out->w = v->w;
}

/* 0x122790: rotates v about the X axis; same w behaviour as Vec3_RotateAxis. */
void Vec3_RotateX(Vec4 *out, Vec4 *v, f32 angle) {
    Vec4 a;
    Vec4 b;
    Vec4 c;
    f32 cs;
    f32 sn;

    cs = Mathf_Cos(angle);
    sn = Mathf_Sin(angle);
    Vec4_Scale(&a, v, cs);
    b.x = 0.0f;
    b.y = -v->z * sn;
    b.z = v->y * sn;
    b.w = 0.0f;
    c.x = v->x * (1.0f - cs);
    c.y = 0.0f;
    c.z = 0.0f;
    c.w = 0.0f;
    Vec4_Add(out, &a, &b);
    Vec4_Add(out, out, &c);
    out->w = v->w;
}

/* 0x122868: rotates v about the Y axis; same w behaviour as Vec3_RotateAxis. */
void Vec3_RotateY(Vec4 *out, Vec4 *v, f32 angle) {
    Vec4 a;
    Vec4 b;
    Vec4 c;
    f32 cs;
    f32 sn;

    cs = Mathf_Cos(angle);
    sn = Mathf_Sin(angle);
    Vec4_Scale(&a, v, cs);
    b.x = v->z * sn;
    b.y = 0.0f;
    b.z = -v->x * sn;
    b.w = 0.0f;
    c.x = 0.0f;
    c.y = v->y * (1.0f - cs);
    c.z = 0.0f;
    c.w = 0.0f;
    Vec4_Add(out, &a, &b);
    Vec4_Add(out, out, &c);
    out->w = v->w;
}
