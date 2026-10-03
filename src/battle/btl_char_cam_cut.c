#include "common.h"
#include "battle/btl_char_cam_int.h"

/* The fighter camera's cut evaluator, 0x1C4F68-0x1C5840: the middle of btl_char_cam.c, on its own until it
 * matches (see the note at the top of btl_char_cam.c). Its two float constants (0x2FD08C, 0x2FD090) stay in the
 * assembly data. */

/* NOT MATCHING (248 of 565 instructions differ): the control flow, calls, stack layout and float code are the same as
   the original; the differences are register allocation in the four "resolve a node" blocks. The original keeps
   the node id in s1 and the masked id in s0 (computed after BtlChar_Get(0) returns) and re-materialises &tmp0
   with addiu in v0 / v1; this C puts the node id and the masked id both in s0 (computed before the call) and
   keeps &tmp0 in s1. */
#if 0
void ChrCam_CalcCut(ChrCamChr *chr, Vec4 *eye, Vec4 *rot, Vec4 *target) {
    Vec4 look;
    Vec4 dir;
    Vec4 tmp0;
    Vec4 tmp1;
    Vec4 tmp2;
    Vec4 tmp3;
    ChrCamCut *cut = &chr->cut;
    s32 frozen;
    ChrCam *cam;
    s32 refreshed;
    s32 usedOpp;
    s32 usedMid;
    f32 t;
    s32 node;
    s32 other;
    s32 id;
    f32 eps;
    f32 yaw;
    f32 pitch;
    f32 dist;
    f32 len;
    f32 limit;
    f32 a;

    frozen = BtlChar_TestFlag(chr, 0xB4);
    cam = &chr->cam;
    t = 0.0f;
    refreshed = 0;
    usedOpp = 0;
    usedMid = 0;
    if (cut->total > 0) {
        t = 1.0f - (f32)cut->timer / (f32)cut->total;
    }

    node = cut->unk88;
    if (node >= 0) {
        other = 0;
        if (node & CHRCUT_NODE_OPP) {
            usedOpp = 1;
            id = BtlOpp_GetObjId(chr);
            other = 1;
        } else if (node & CHRCUT_NODE_MID) {
            id = -1;
            usedMid = 1;
        } else {
            id = chr->objId;
        }
        if (id < 0) {
            node &= CHRCUT_NODE_MASK;
            func_002058E0(BtlChar_Get(0)->objId, node, &tmp0);
            func_002058E0(BtlChar_Get(1)->objId, node, &tmp1);
            func_00122168(&cut->vecA, &tmp0, &tmp1, 0.5f);
        } else if (frozen == 0 || other == 0) {
            func_002058E0(id, cut->unk88 & CHRCUT_NODE_MASK, &cut->vecA);
        }
        refreshed = 1;
    }

    node = cut->unk8C;
    if (node >= 0) {
        if (cut->unk88 == node) {
            func_00121E18(&cut->vecADelta);
        } else {
            other = 0;
            if (node & CHRCUT_NODE_OPP) {
                usedOpp = 1;
                id = BtlOpp_GetObjId(chr);
                other = 1;
            } else if (node & CHRCUT_NODE_MID) {
                id = -1;
                usedMid = 1;
            } else {
                id = chr->objId;
            }
            if (id < 0) {
                node &= CHRCUT_NODE_MASK;
                func_002058E0(BtlChar_Get(0)->objId, node, &tmp2);
                func_002058E0(BtlChar_Get(1)->objId, node, &tmp3);
                func_00122168(&tmp0, &tmp2, &tmp3, 0.5f);
            } else if (frozen == 0 || other == 0) {
                func_002058E0(id, cut->unk8C & CHRCUT_NODE_MASK, &tmp0);
            }
            Vec4_Sub(&cut->vecADelta, &tmp0, &cut->vecA);
        }
        refreshed = 1;
    }

    node = cut->unk90;
    if (node >= 0) {
        other = 0;
        if (node & CHRCUT_NODE_OPP) {
            id = BtlOpp_GetObjId(chr);
            other = 1;
        } else if (node & CHRCUT_NODE_MID) {
            id = -1;
        } else {
            id = chr->objId;
        }
        if (id < 0) {
            node &= CHRCUT_NODE_MASK;
            func_002058E0(BtlChar_Get(0)->objId, node, &tmp0);
            func_002058E0(BtlChar_Get(1)->objId, node, &tmp1);
            func_00122168(&cut->vecC, &tmp0, &tmp1, 0.5f);
        } else if (frozen == 0 || other == 0) {
            func_002058E0(id, node & CHRCUT_NODE_MASK, &cut->vecC);
        }
    }

    node = cut->unk94;
    if (node >= 0) {
        if (cut->unk90 == node) {
            func_00121E18(&cut->vecCDelta);
        } else {
            other = 0;
            if (node & CHRCUT_NODE_OPP) {
                id = BtlOpp_GetObjId(chr);
                other = 1;
            } else if (node & CHRCUT_NODE_MID) {
                id = -1;
            } else {
                id = chr->objId;
            }
            if (id < 0) {
                node &= CHRCUT_NODE_MASK;
                func_002058E0(BtlChar_Get(0)->objId, node, &tmp1);
                func_002058E0(BtlChar_Get(1)->objId, node, &tmp2);
                func_00122168(&tmp0, &tmp1, &tmp2, 0.5f);
            } else if (frozen == 0 || other == 0) {
                func_002058E0(id, node & CHRCUT_NODE_MASK, &tmp0);
            }
            Vec4_Sub(&cut->vecCDelta, &tmp0, &cut->vecC);
        }
    }
    if (refreshed) {
        if (usedOpp) {
            Vec4_Copy(&cut->vecB, *(Vec4 **)((u8 *)BtlOpp_GetObj(chr) + 0xFA0));
        } else if (usedMid) {
            Vec4_Copy(&tmp0, *(Vec4 **)((u8 *)BtlOpp_GetObj(chr) + 0xFA0));
            Vec4_Copy(&tmp1, *(Vec4 **)((u8 *)BtlChar_GetObj(chr) + 0xFA0));
            func_00122168(&cut->vecB, &tmp0, &tmp1, 0.5f);
        } else {
            Vec4_Copy(&cut->vecB, *(Vec4 **)((u8 *)BtlChar_GetObj(chr) + 0xFA0));
        }
        func_00121E18(&cut->vecBDelta);
    }

    eps = 0.001f;
    Vec4_Scale(eye, &cut->vecADelta, t);
    Vec4_Add(eye, &cut->vecA, eye);
    Vec4_Scale(target, &cut->vecBDelta, t);
    Vec4_Add(target, &cut->vecB, target);
    Vec4_Scale(&look, &cut->vecCDelta, t);
    Vec4_Add(&look, &cut->vecC, &look);
    yaw = cut->valA + cut->valADelta * t;
    pitch = -(cut->valB + cut->valBDelta * t);
    dist = cut->valC + cut->valCDelta * t;
    dir.x = Mathf_Cos(pitch) * Mathf_Sin(yaw);
    dir.y = Mathf_Sin(pitch);
    dir.z = Mathf_Cos(pitch) * Mathf_Cos(yaw);
    Vec4_Scale(&dir, &dir, dist);
    Vec4_Sub(eye, eye, &dir);
    func_00121E20(rot);
    Vec4_Sub(&dir, &look, eye);
    len = Vec3_Length(&dir);
    if (eps < len) {
        Vec4_Scale(&dir, &dir, 1.0f / len);
        func_00122140(&dir, &dir, -1.0f, 1.0f);
        rot->x = Mathf_Asin(-dir.y);
        if (__builtin_fabsf(dir.x) < eps && __builtin_fabsf(dir.z) < eps) {
            rot->y = 0.0f;
        } else {
            rot->y = atan2f(dir.x, dir.z);
        }
    }

    if (cut->flags & CHRCUT_F_SNAP_RUN) {
        BtlChar_SetFlag(chr, 0xCD);
    }
    if (cut->flags & CHRCUT_F_RATE_RUN) {
        BtlChar_SetFlag(chr, 0xD1);
    }
    if (cut->flags & CHRCUT_F_PRIORITY) {
        BtlChar_SetFlag(chr, 0xD3);
    }
    if (cut->timer > 0) {
        cut->timer--;
        if (cut->timer == 0 && !(cut->flags & CHRCUT_F_HOLD)) {
            if (cut->flags & CHRCUT_F_SNAP_END) {
                BtlChar_SetFlag(chr, 0xCE);
            }
            ChrCam_EndCut(chr);
        }
    }
    if (cut->flags & CHRCUT_F_SET_SIDE) {
        Vec4_Sub(&tmp2, &BtlChar_GetPos(chr)->pos, &cam->eye);
        if (0.001f < __builtin_fabsf(tmp2.x) || 0.001f < __builtin_fabsf(tmp2.z)) {
            limit = ChrCam_GetSideLimit(chr);
            a = atan2f(tmp2.x, tmp2.z);
            chr->cam.side = BtlUtil_ClampF(-BtlUtil_WrapAngle(a - BtlChar_GetPos(chr)->yaw), -limit, limit);
        }
    }
    BtlChar_SetFlag(chr, 0xD4);
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/btl_char_cam_cut", ChrCam_CalcCut);
#endif
