# Effects and stage (seventh batch, in progress)

Sources: `src/battle/eft_*.c`, `src/battle/stg_*.c` with headers under `include/battle/`.
None of it is linked yet; "match" below means per function. **Verified** = matching C;
**inferred** = read from disassembly or a judgement about meaning.

The question this batch answers for the port: which of this code affects the fight
(simulation) and which only draws.

## Screen effects (`stg_c.c`, 0x245F58..0x248F28): visual only

66 of 68 functions match (`StgHaze_Draw` and `StgBlur_Draw`, both GS packet code, stay in
assembly). Despite its address this is not stage geometry: it is the battle's full-screen post
effects and the manager that creates, resets and draws them. No function touches fighters, hit
records, battle objects, rigid bodies or collision. (verified)

| Group | What |
|---|---|
| `StgCurve_*` | a 3-key Hermite curve sampled into 256 entries (used by fog and haze) |
| `StgHaze_*` | depth haze / heat shimmer, one per view, on stages with feature 9 |
| `StgTint_*` | four full-screen colour layers with linear ramps |
| `StgBlur_*` | radial zoom blur, one per view |
| `StgFx_*` | the group manager: init, term, reset, four draw passes |

- Draw order in a battle frame: stage, `StgFx_DrawPre`, (0x10FF40), effect scene per view,
  `StgFx_DrawPost` (white stage-change fade, blur), HUD, `StgFx_DrawOverlay` (cross-fade,
  front tints). The group has no update call: **its timers advance once per drawn frame**.
- Fighter effect request 0x12 is the stage darkening: tint slot 0, black at the stage file's
  alpha, over 0.2 s, ramped back when no fighter has the request.
- Tint slots 0..2 freeze while paused; slot 3 does not.
- **The haze draws libc `rand()` from inside a draw call**: 2 per mesh vertex per view (2000
  per frame with default parameters), only on stages with feature 9 and not while paused; the
  count depends on split screen and the demo camera. Appearance only, but it consumes the
  shared libc stream that reaches the double-KO tie-break (netplay_notes.md).
- Original bug (inferred): `StgHaze_SetParams` writes the depth curve only into view 0, so
  view 1 has an all-zero curve on stages with a haze section.
- Stage file sections identified: fog, tint colours (two RGBA), haze parameters.

Still without an owner: the stage's rigid bodies, debris and destructibles (0x22FDA0..0x230AA0
and the stage update at 0x243568).

## Module table (filled as agents report)

| Range | File | Module | Simulation? | Random draws | Match |
|---|---|---|---|---|---|
| 0x245F58..0x248F28 | stg_c.c | screen effects (haze, tints, blur) | no | libc `rand()` in a draw pass (haze) | 66/68 |
| 0x15F728..0x1609C8 | eft_m.c | speed lines `EftSpdLine_*` (30 trails, 40 streaks; task table `D_002C3A18`) | no | libc `rand()` in update / spawn | 20/20 |
| 0x1609C8..0x1637A0 | eft_m.c | aura particles `EftAura_*`, first half (flames from 10 body parts, sparks from 12 emitters; pools shared by all fighters) | no | libc `rand()` in update / spawn, count depends on live particles | 30/30 |
| 0x142CA0..0x147050 | eft_f.c | water-surface particles, second half of the water module in eft_e.c (pools: 15 trails, 60 drops, 30 rings, 30 sprays, 30 mists) | no (reads a hit record's position, fighter height, water height) | libc `rand()`: 19 per blast trail, **not spawned in split screen** | 35/40 |

## Notes common to effect modules

- (verified, eft_m) Effect task tables have the shape `{update, init, term, stub, 0, draw}` and
  are listed in `D_002C3FB0` as pairs `{table, 1}`.
- (verified, eft_m) The effect code's vector type is 16-byte aligned and passed by value with a
  callee copy (`ld` / `sd` pairs at function entry); `Vec4` from sys/math3d.h does not
  reproduce that, so effect files use a local aligned type.
- (verified) Visual modules read fighters only through the read-only `BtlCharApi_*` getters.
- (verified) Visual modules consume libc `rand()` at a rate that depends on how many particles
  are alive and on the pause flag. Since libc `rand()` also reaches simulation (double-KO
  tie-break, camera shake), a port must give visual effects their own generator.
- (inferred hazard, eft_m) `EftAura_StepFlames` revisits the same flame forever if the flame
  pool is exhausted when an expired flame tries to spawn its successor.
- (verified, eft_f / eft_e) Blast trails on water are not spawned in split screen, so the libc
  `rand()` call count depends on the screen mode: another reason for a separate generator.
- (verified, eft_f) Effect particles step by a fixed 1/30 s; durations are `seconds * 30`
  frames; updates do nothing while paused (battle flag 0x100).
- (evidence) eft_e.c and eft_f.c were one source file: `EftWaterRing_Update` matches only with
  `EftWater_GetSurfaceY` (0x140338) defined earlier in the same file.
