# Maths and random numbers

Sources: `src/sys/math3d.c`, `src/sys/rand.c`. Layouts: `include/sys/math3d.h`,
`include/sys/rand.h`.

## Conventions (verified where a matching function shows them)

- `Vec4 {x, y, z, w}`, 0x10 bytes. Vectors are four floats even when only xyz is used; locals
  sit 0x10 apart and are passed to the VU0 `Vec3_*` routines.
- `Quat {x, y, z, w}`, w at +0xC.
- `Mtx44`, 0x40 bytes, row-vector convention (`v' = v * M`): `Quat_ToMtx` writes
  `m[0][1] = 2(xy + wz)` and `m[1][0] = 2(xy - wz)`. That translation is row 3 is inferred.
- All game maths is single-precision float. `double` would go through software routines.

## Quaternion and spline routines (verified)

`Vec3_Hermite`, `Quat_SetIdentity`, `Quat_FromAxisAngle`, `Quat_FromVectors`, `Quat_LengthSq`,
`Quat_Length`, `Quat_SlerpIdentity`, `Quat_GetAngle`, `Quat_Conjugate`, `Quat_Inverse`,
`Quat_Mul` (Hamilton product), `Quat_Slerp` (shorter arc), `Quat_ConjugateBy`,
`Quat_LimitAngle`, `Quat_Normalize`, `Quat_Pack`, `Quat_Unpack`, `Quat_ToMtx`,
`Quat_FromEuler` (qy * (qx * qz)), `Mtx_ToEuler`, `Quat_ToEuler`, `Quat_FromMtx`.

`Quat_Pack` / `Quat_Unpack`: a quaternion in 64 bits. The largest component is dropped; the
other three are stored as 20-bit fields scaled by sqrt(2) / 1048575; the index of the dropped
component is in bits 60..63. `Quat_Unpack` has 10 call sites (0x24BF58, 0x24C1A8, 0x1FC1E8,
0x1FCEF0), which look like animation key decoding (inferred).

## VU0 vector routines (not decompiled)

About 200 small functions at 0x11FA10..0x123130 are the vector library; many use VU0 (the PS2's vector coprocessor) and are
hand-written or inline assembly in the original. The disassembler emits several of them as raw
words. Names so far (`Vec4_Copy`, `Vec3_Normalize`, `Vec3_Cross`, `Vec3_Dot`, `Vec3_Length`,
`Mtx_StoreIdentity`, ...) are from call patterns and are partly guesses. `func_0011F780` and
`func_0011F740` are acos and asin with the argument clamped to [-1, 1].

## Scalar helpers (`src/sys/mathf.c`; verified)

- `Mathf_Sin(a) = sinf(wrap(a))`, `Mathf_Cos(a) = Mathf_Sin(a + pi/2)`: 101 and 78 call sites.
- The wrap (`Mathf_WrapAngle`, hand-written): `r = 2*pi; while (a < r) a += r; while (pi < a)
  a -= r;`. An intended lower bound is overwritten, so every angle below 2 pi is pushed up and
  brought back. (inferred) In-range angles are quantised to the float spacing near 2 pi, and a
  huge angle never terminates.
- `Mathf_SinFast` / `Mathf_CosFast` (18 / 20 sites): the polynomial
  `x + c3 x^3 + c5 x^5 + c7 x^7 + c9 x^9` on the vector unit. Different bits from the pair
  above.
- `Mathf_Tan` wraps correctly by 2 pi steps, then `tanf`. `Mathf_Asin` / `Mathf_Acos` clamp to
  [-1, 1] first. `Mathf_Sqrt` is the vector unit's square root.
- `BtlUtil_WrapAngle` (battle side) adds or subtracts 2 pi once.

## Random numbers

Seven sources; the table with reset points and users is in netplay_notes.md.

1. `BtlChar_Rand` and `BtlScene_Rand`: `(state * 714025 + 4096) % 150889`, separate states,
   reset with the fighters and with the effect scene. (verified)
2. `BtlChar_FrameMod(n)`: the fight's frame counter modulo n. (verified)
3. libc `rand()`: about 490 direct call sites, the main generator for effects; seeded at boot
   from a timer. `Rand_IntRange(a, b) = lo + rand() % (hi - lo + 1)`. (verified)
4. `Rand_Float01` / `Rand_FloatRange`: the vector unit's R register (7 steps, re-init from its
   own output, 7 more steps, minus 1.0, giving [0, 1) in 23 bits). Seeded once at boot from a
   constant, so the stream is the same every boot. (verified) The usual model of the register
   is a 23-bit shift register with feedback from bits 4 and 22. (inferred, not from this
   binary)
5. `Rand_*`: MT19937 seeding and tempering, but the refill only runs the first loop and the
   final word; 396 of 624 state words are never regenerated. Used by the AI and the menus.
   (verified)
6. A second MT19937 copy at 0x252F68, a menu codec seeded per call. (inferred)

## Float constants and matching

The compiler truncates decimal literals: `3.1415925f` gives 0x40490FD9, one bit below the
original's 0x40490FDA; `3.14159265f` is needed. Same for sqrt(2). The diff tool masks constant
relocations, so each emitted constant has to be checked against the original data by value.
