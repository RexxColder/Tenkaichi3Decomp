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
