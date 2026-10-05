/*
 * The PS2's single-precision arithmetic for the whole game.
 *
 * Every game source of the PC build is compiled with -msoft-float: the compiler emits no float instruction at all
 * and calls these routines for each float operation, with floats travelling as 32-bit patterns in integer
 * registers. The routines implement the Emotion Engine's float unit (the model of src/port/vu0_a.c): no NaN, no
 * infinity, no denormals, results truncated, division by zero gives the largest number. So the simulation does
 * the same arithmetic on every host, and none of the host's float behaviour (rounding mode, x87, NaN handling)
 * can leak into it. Doubles (rare in the game; software IEEE on the PS2 as well) are in plat_libm.c.
 *
 * NOT verified against a console; the open points are listed in docs/systems/math.md of the decompilation.
 */
#include <stdint.h>
#include "port/vu0_a.h"

#define SIGN 0x80000000u

uint32_t __addsf3(uint32_t a, uint32_t b) { return RefVu0_AddBits(a, b); }
uint32_t __subsf3(uint32_t a, uint32_t b) { return RefVu0_SubBits(a, b); }
uint32_t __mulsf3(uint32_t a, uint32_t b) { return RefVu0_MulBits(a, b); }
uint32_t __divsf3(uint32_t a, uint32_t b) { return RefVu0_DivBits(a, b); }
uint32_t __negsf2(uint32_t a) { return a ^ SIGN; }

/* Comparisons: every pattern is an ordinary number (c.eq.s / c.lt.s / c.le.s never see "unordered"). */
static int cmp(uint32_t a, uint32_t b) {
    return RefVu0_LtBits(a, b) ? -1 : RefVu0_LtBits(b, a) ? 1 : 0;
}
int __eqsf2(uint32_t a, uint32_t b) { return cmp(a, b); }
int __nesf2(uint32_t a, uint32_t b) { return cmp(a, b); }
int __ltsf2(uint32_t a, uint32_t b) { return cmp(a, b); }
int __lesf2(uint32_t a, uint32_t b) { return cmp(a, b); }
int __gtsf2(uint32_t a, uint32_t b) { return cmp(a, b); }
int __gesf2(uint32_t a, uint32_t b) { return cmp(a, b); }
int __unordsf2(uint32_t a, uint32_t b) { (void)a; (void)b; return 0; }

/* cvt.w.s: truncates; out of range gives the largest / smallest integer. */
int32_t __fixsfsi(uint32_t a) {
    int32_t e = (int32_t)((a >> 23) & 0xFF) - 127;
    uint32_t m = (a & 0x7FFFFF) | 0x800000;
    uint32_t v;

    if (e < 0) {
        return 0;
    }
    if (e >= 31) {
        return (a & SIGN) ? (int32_t)0x80000000u : 0x7FFFFFFF;
    }
    v = e >= 23 ? m << (e - 23) : m >> (23 - e);
    return (a & SIGN) ? -(int32_t)v : (int32_t)v;
}

/* The PS2 compiler's float -> unsigned: values of 2^31 and above go through (x - 2^31) and get the top bit. */
uint32_t __fixunssfsi(uint32_t a) {
    if (!RefVu0_LtBits(a, 0x4F000000u)) {
        return (uint32_t)__fixsfsi(RefVu0_SubBits(a, 0x4F000000u)) | SIGN;
    }
    return (uint32_t)__fixsfsi(a);
}

/* cvt.s.w: truncates to 24 bits. */
uint32_t __floatsisf(int32_t v) { return RefVu0_ItofBits(v, 0); }

/* unsigned -> float as the PS2 compiler does it: convert as signed, add 2^32 when the top bit was set. */
uint32_t __floatunsisf(uint32_t v) {
    uint32_t f = RefVu0_ItofBits((int32_t)v, 0);

    return (v & SIGN) ? RefVu0_AddBits(f, 0x4F800000u) : f;
}
