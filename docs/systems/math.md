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

About 223 small functions at 0x11F830..0x123F48 use VU0 (the PS2's vector coprocessor) and are
hand-written or inline assembly in the original. The disassembler emits several of them as raw
words. Names so far (`Vec4_Copy`, `Vec3_Normalize`, `Vec3_Cross`, `Vec3_Dot`, `Vec3_Length`,
`Mtx_StoreIdentity`, ...) are from call patterns and are partly guesses. `func_0011F780` and
`func_0011F740` are acos and asin with the argument clamped to [-1, 1].

## Random numbers

There are three generators:

1. **`Rand_*` (verified):** MT19937 seeding and tempering, but the state refill only runs the
   first loop (kk = 0..226) and the final wrap-around word. The reference code's second loop
   (kk = 227..622) is absent, so 396 of the 624 state words are never regenerated. The output is
   not a true MT19937 sequence. `Rand_Init` seeds from `rand()` values after
   `srand(clock-like timer)`. `Rand_Range(n)` is `Rand_Next() % n` (0 when n is 0), unsigned;
   263 call sites.
2. **C library `rand()`:** used by the battle sequence (intro and win line choice, the
   tie-break in `BtlSeq_JudgeByHealth`) and to seed generator 1.
3. **A second MT19937 copy at 0x252F68** with its own state, followed by a 32-byte bit buffer
   (not decompiled). It looks like a scramble or password codec (inferred).

## Float constants and matching

The compiler truncates decimal literals: `3.1415925f` gives 0x40490FD9, one bit below the
original's 0x40490FDA; `3.14159265f` is needed. Same for sqrt(2). The diff tool masks constant
relocations, so each emitted constant has to be checked against the original data by value.
