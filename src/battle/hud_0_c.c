#include "common.h"
#include "battle/hud_0.h"

/*
 * Battle pause menu, part 3: head of the skill-list text module. Source range 0x215010-0x215420.
 *
 * These seven functions belong with the BtlText_* code that starts src/battle/btl_seq.c at 0x215420 (same
 * sprite sheet, same callers). Unlike the menu engine before them they reach the menu work through
 * BtlMenu_GetWork() and never through the global, which is the evidence for a file boundary at 0x215010.
 *
 * Presentation only: no simulation state, no pad, no random numbers.
 */

/* Local view of the 0x7FC-byte progress block: only the control configuration is used. */
typedef struct BtlTextProgress {
    /* 0x000 */ u8 unk0[0x7F4];
    /* 0x7F4 */ s32 controlType;
} BtlTextProgress;

extern BtlTextProgress *gProgress;

extern void Font_PushStyle(void);
extern void Font_PopStyle(void);
extern void Font_SetScale(f32 scale);
extern void Font_SetSpacing(s32 x, s32 y);
extern void Font_SetShadowMode(s32 mode);
extern void Font_SetColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_SetShadowColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_PrintAt(s32 x, s32 y, u16 *str);

/* Draws the controls page inside a box: the title (sheet entry 0, row 4) centred at the top and the text for
   the pad type and the control configuration (gProgress + 0x7F4) below it. */
void BtlText_DrawControls(u64 **pkt, s32 x0, s32 x1, s32 y0, s32 y1) {
    s32 config = gProgress->controlType;
    s32 padType = BtlMenu_GetDefaultPadType();

    Font_PushStyle();
    Font_SetShadowMode(2);
    Font_SetSpacing(1, 4);
    BtlMenu_DrawPart(pkt, x0 + (x1 - x0) / 2, y0 + 0xE, 0x100, 0x20, 0, 4, 1);
    Font_SetScale(1.0f);
    Font_SetColorRGBA(0xFF, 0xFF, 0xFF, 0x80);
    Font_SetShadowColorRGBA(0, 0x10, 0x10, 0x40);
    Font_PrintAt(x0 + 0x10, y0 + 0x2A, BtlMenu_GetControlsText(padType, config));
    Font_PopStyle();
}

/* 1 when a box in GS coordinates lies inside the drawing area. */
s32 BtlText_IsOnScreen(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 result = 0;

    if (x0 > 0 && x1 < 0x1000 && y0 > 0 && y1 < 0x1000) {
        result = 1;
    }
    return result;
}

/*
 * Writes one sprite of sheet entry `part`: corners in screen pixels, UV in texels. Nothing is written when
 * the box is off screen. The packet is the same as BtlMenu_PutSprite's: a REGLIST GIF tag with 8 registers
 * (PRIM, NOP, TEX0_1, RGBAQ, UV, XYZ2, UV, XYZ2), PRIM 0x156 (sprite, textured, blended, UV coordinates),
 * TEX0, the colour with Q = 1.0f, then the two corners with Z = 0xFFFFFFF0. Screen offset: x + 0x700,
 * y + 0x720 (in 1/16 pixel after the shift by 4).
 *
 * NOT MATCHING (7 of 108 instructions; registers only). The attempt is the same code with two saved registers
 * exchanged: the original keeps x0 + 0x700 in s4 and u0 in s3, this compiler output has them in s3 and s4.
 * The allocator orders the two by (uses / live length): 3 / 49 against 2 / 33 instructions here, and the
 * original needs x0 one instruction longer-lived (or u0 one shorter); nothing tried changes that without
 * also changing the order the two XYZ halves are built in (statement order of the four offset additions, the
 * operand order of the ORs, locals for the corners / the UVs / the Z constant, helper functions, an early
 * return). The same pattern resists in FontIcon_PutSprite (col_c_b.c). Checked with the attempt enabled:
 * every other function of the file prints OK either way.
 * Second cleanup pass: the exact condition is u0 one instruction shorter-lived (2 / 32 then ties with y0, which
 * wins as the older register) or x0 AND y0 one longer (the Z constant scheduled in front of their sign
 * extensions by the first scheduling pass). Writing the UV word as `((s64)v0 << 20) | ((s64)u0 << 4)` puts x0
 * in s4 but exchanges u0 and v0 (s5 / s3: 4 differences). Without effect: a local copy of u0 (propagated away),
 * the colour word written five other ways, a variable for the Z constant at four places, the constant first.
 */
#if 0
void BtlText_PutSprite(u64 **pkt, s32 x0, s32 y0, s32 x1, s32 y1, s32 u0, s32 v0, s32 u1, s32 v1, u32 color,
                       s32 part) {
    x0 += 0x700;
    x1 += 0x700;
    y0 += 0x720;
    y1 += 0x720;
    if (BtlText_IsOnScreen(x0, y0, x1, y1)) {
        (*pkt)[0] = 0x8400000000008001;
        (*pkt)[1] = 0x535316F0;
        *pkt += 2;
        (*pkt)[0] = 0x156;
        (*pkt)[1] = 0;
        *pkt += 2;
        (*pkt)[0] = BtlText_GetTex0(part);
        (*pkt)[1] = (u64)color | 0x3F80000000000000;
        *pkt += 2;
        (*pkt)[0] = ((s64)u0 << 4) | ((s64)v0 << 20);
        (*pkt)[1] = ((s64)x0 << 4) | ((s64)y0 << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = ((s64)u1 << 4) | ((s64)v1 << 20);
        (*pkt)[1] = ((s64)x1 << 4) | ((s64)y1 << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
    }
}
#endif
INCLUDE_ASM("asm/nonmatchings/battle/hud_0_c", BtlText_PutSprite);

/* TEX0 of sheet entry `part` (its own pixels and CLUT, TCC on). */
u64 BtlText_GetTex0(s32 part) {
    BtlMenuTex *t = &BtlMenu_GetWork()->tex->ent[part];
    s32 cbp = t->cbpOfs;
    s32 tbp = t->tbpOfs;
    u64 tex0 = t->tex0;

    tex0 |= (u64)(cbp + 0x2A00) << 37;
    tex0 |= tbp + 0x2A40;
    tex0 |= 0x400000000;
    return tex0;
}

/* Writes SCISSOR_1 for a clip rectangle (inclusive pixel bounds). */
void BtlText_PutScissor(u64 **pkt, s32 x0, s32 y0, s32 x1, s32 y1) {
    (*pkt)[0] = 0x1000000000008001;
    (*pkt)[1] = 0xE;
    *pkt += 2;
    (*pkt)[0] = (((s64)x0 | ((s64)x1 << 16)) | ((s64)y0 << 32)) | ((s64)y1 << 48);
    (*pkt)[1] = 0x40;
    *pkt += 2;
}

/* 1 for a line feed or a carriage return. */
s32 BtlText_IsNewline(u16 c) {
    s32 result = 0;

    if (c == 10 || c == 13) {
        result = 1;
    }
    return result;
}

/* Returns the pointer behind the next newline character, or to the terminator when the text ends first. */
u16 *BtlText_NextLine(u16 *p) {
    while (*p != 0) {
        if (BtlText_IsNewline(*p)) {
            p++;
            break;
        }
        p++;
    }
    return p;
}
