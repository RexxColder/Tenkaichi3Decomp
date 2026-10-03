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
| 0x12DD80..0x12F550 | eft_a.c | **hit record list `EftHit_*`** | **yes** | none | (eft_a 80/90) |
| 0x12F550..0x12F810 | eft_a.c | technique camera cut `EftCam_*` (drives the demo camera) | camera only | none | |
| 0x12F810..0x131030 | eft_a.c | shared helpers: palette lighting, splines, `EftMath_WrapAngle`, **projectile aim and homing `EftAim_*`**, clip planes | aim and homing: **yes** | none | |
| 0x1312A8..0x132290 | eft_a.c | clipped polygon and sprite drawing `EftGfx_*` | no | none | |
| 0x14F230..0x151AD8 | eft_i.c | effect pack library `EftEmit_*` (continues eft_h): spawns, moves and kills the part objects of 11 part modules; six node slots per pack | drives parts; no hit records itself | libc `rand()`: 2 per started part with a spread (reaches the part's spawn position) | (eft_i 36/43) |
| 0x151AD8..0x152978 | eft_i.c | **technique effect type 3 `EftSweep*`: a sweeping beam** | **yes**: one hit record per frame; traces the stage; **destroys stage objects** (`BtlStage_DestroyObj`); sets fighter flag 0xA8 at its end | none | |
| 0x152978..0x1532A0 | eft_i.c | **technique effect type 7 `EftFollow*`: an effect on fighter node 3** | **yes**: a two-sphere hit record per frame; sets flag 0xA8 at its end. Also drives the stage blur light (visual) | none | |
| 0x1532A0..0x1533B0 | eft_j.c | tail of the type 7 follow effect | (see eft_i) | none | (eft_j 64/65) |
| 0x1533B0..0x1542A8 | eft_j.c | type 5 `EftMulti*`: fires up to ten pieces (separate tasks at 0x16D858.. that carry the hits) | **yes** (creates and steers pieces) | libc `rand()`: one per piece for effect id 0x202 (an angle handed to the piece; effect on the hit not verified) | |
| 0x1542A8..0x155588 | eft_j.c | type 6 `EftPropShot*`: one shot carrying a model | **yes**: hit record per frame; sets fighter flag 0xA8 and an object flag | `BtlScene_RandF`: two at init for id 0x165 (model bob, appearance) | |
| 0x155588..0x156450 | eft_j.c | **type 0 `EftBlast*`: the plain blast / beam** | **yes**: hit record per frame | none | |
| 0x156450..0x157398 | eft_j.c | type 2 `EftShotTech*` helpers: up to 14 blast objects (tasks at 0x16A7D0..) | **yes** (creates, retargets and stops them) | none | |
| 0x132290..0x1333C8 | eft_b.c | primitive helpers `EftPrim_*` (quads, triangles) | no | none | (eft_b 43/49) |
| 0x1333C8..0x135070 | eft_b.c | underwater bubbles `EftBubble_*` (pool of 100; layer 0 sub-task 7) | no | libc `rand()`; **count depends on camera pose** (per frame while a view's camera is under water), none in split screen; ambient body bubbles only for object 0 | |
| 0x135070..0x135610 | eft_b.c | scrolling stage sheet `EftStageScroll_*` (sub-task 0) | no | libc `rand()` once at load | |
| 0x135610..0x136760 | eft_b.c | geyser columns `EftGeyser_*` at stage-defined positions (sub-task 6) | no (its two emitters, in other files, not classified) | libc `rand()` once per column at creation | |
| 0x13EA00..0x13F3D8 | eft_e.c | **stage-change transition `EftBurst_*` (scene layer 4)**: demo-camera animation, a model, 350 particles on a fixed schedule | no state writes, but **the stage swap waits on it** (150 unpaused frames) | libc `rand()` every unpaused frame | (eft_e 47/49) |
| 0x13F430..0x140338 | eft_e.c | stage particle emitters `EftSteam_*` (layer 0 sub-task 5; also used by the geysers) | no | VU0 register: 7 per new particle; emission is not gated by pause | |
| 0x140338..0x142CA0 | eft_e.c | water surface `EftWater_*`, first half (splashes, wakes; continues in eft_f) | one bit: sets flag 0x400 on a hit record's task (inferred private) | libc `rand()`: **one per eligible hit record per frame**, 61 / 44 per splash, 19 / 6 per frame per wake; **all creation is off in split screen** | |
| 0x242D28..0x2435C0 | stg_b.c | stage data readers `BtlStage_*`, stage timers, `BtlStage_Update` | timers and flags only (readers elsewhere) | none | (stg_b 84/88) |
| 0x2435C0..0x244170 | stg_b.c | stage ambience sound `StgAmb_*` (23 per-stage volume handlers) | no | libc `rand()` on stages 3, 4, 10, 15, 27 (random one-shot sounds) | |
| 0x244170..0x244890 | stg_b.c | screen cross-fade `ScrXfade_*` | no | none | |
| 0x244890..0x245878 | stg_b.c | screen shock waves `ScrWarp_*` (10 rings) | no | VU0 register: 20 per spawn | |
| 0x245878..0x245F58 | stg_b.c | depth blur `StgFog_*` (continues in stg_c) | no | none | |
| 0x245F58..0x248F28 | stg_c.c | screen effects (haze, tints, blur) | no | libc `rand()` in a draw pass (haze) | 66/68 |
| 0x15F728..0x1609C8 | eft_m.c | speed lines `EftSpdLine_*` (30 trails, 40 streaks; task table `D_002C3A18`) | no | libc `rand()` in update / spawn | 20/20 |
| 0x1609C8..0x1637A0 | eft_m.c | aura particles `EftAura_*`, first half (flames from 10 body parts, sparks from 12 emitters; pools shared by all fighters) | no | libc `rand()` in update / spawn, count depends on live particles | 30/30 |
| 0x142CA0..0x147050 | eft_f.c | water-surface particles, second half of the water module in eft_e.c (pools: 15 trails, 60 drops, 30 rings, 30 sprays, 30 mists) | no (reads a hit record's position, fighter height, water height) | libc `rand()`: 19 per blast trail, **not spawned in split screen** | 35/40 |
| 0x147050..0x147928 | eft_g.c | helpers (facing matrix, water clip, blast record class) | no | none | (eft_g 63/71) |
| 0x147928..0x148DF8 | eft_g.c | storm `EftStorm_*`: 4 lightning bolts, 64 rain drops (scene layer 0, sub-task 3) | no | libc `rand()`; **rain re-rolls inside the draw pass per view**, so the count depends on camera pose and split screen | |
| 0x148DF8..0x149818 | eft_g.c | stage smoke emitters `EftSmoke_*` (sub-task 4), 40 particles each | no | libc `rand()` and the VU0 register per particle | |
| 0x149818..0x14A828 | eft_g.c | stage boundary wall `EftBound_*` (sub-task 8), a mesh on the stage cylinder near each fighter | no (reads fighter state) | none | |
| 0x14A828..0x14B108 | eft_g.c | **shot slots `EftShot_*`: start of scene layer 1** | **yes** | none | |

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

## Shot layer (simulation; `eft_g.c` from 0x14A828, continues in eft_h)

- (verified) Scene layer 0 holds stage-wide effect sub-tasks registered in `D_002C3568` as
  `{class, index}` pairs. **Scene layer 1 is the shot layer**: `gEftShot` holds one 0x540-byte
  `EftShotChar` per character with 5 slots of 0x50 bytes and 5 definitions of 0x8C bytes. Slot
  kinds 0 and 1 are ki blasts, 2..4 techniques.
- (verified) A hit record's source pointer (+0x64) is a shot slot; the slot points at its pack
  data (+0x1C) and definition (+0x24).
- (verified) `EftShot_Request` (0x14AB90) is what the fighter calls to start a shot.
- (verified) **The effect code ends a beam's firing loop**: `EftShot_SetHeldFlagA8` / `A9` set
  the fighter's held flags 0xA8 / 0xA9, the "beam over" / "second stage over" flags the
  technique handlers wait on (combat.md). Seven callers further into the effect range.
- (verified) Each frame a character's current slot is cleared when the fighter is in neither a
  technique nor a skill. The code uses the character index as the fighter's object id.
- (verified hazard) `EftShotMgr_Init` always creates tasks for characters 0 and 1 and writes
  the second entry whatever the character count is.

## Hit records (simulation core; `eft_a.c`, verified unless marked)

`gEftHitList` is 64 records of 0x190 bytes plus a count; `gEftHitArena` is a 0x1800-byte
per-frame bump allocator for their shapes.

- **A record lives one frame.** `EftHit_BeginFrame` (start of `BtlScene_Update`) empties the
  list; projectile tasks then append records (`EftHit_GetNew`, fill, `EftHit_Add`), one or more
  per projectile per frame. Twelve task modules create records.
- A record has: owner object id, type (1 technique with a source slot and definition, 0 ki
  blast with blast parameters), `pos` / `prevPos` / `vel`, flags, a swept shape (two spheres or
  two boxes), and a pointer to its owning task.
- Feedback goes to the owning task as flags: bit 0 hit a fighter, 0x10 left the stage, 0x40
  dead, 0x200 impact effect spawned, 0x4000 multi-hit, 0x8000 one-shot. `EftHit_UpdateResults`
  (start of `BtlScene_PostUpdate`) condenses them into `task->result` (0, 1, 3, 4 hit,
  5 finished), which decides whether the projectile dies.
- Multi-hit techniques count hits in the task; record flag 0x10 marks the last hit.
- **Clash rule**: two blasts compare `level` (higher wins, equal is a draw); a blast loses to a
  technique; two techniques clash only for certain classes and kinds, and moving head-on gives
  the beam struggle (raised for both sides).
- Stage bound: a record further than stage radius - 100 from the origin gets "left the stage",
  is pulled back onto that radius (its collision shape is rewritten), and finishes.
- Aim: `BtlCharApi_CalcAimDir45` when locked on, else the fighter's yaw; more than 60 degrees
  off the facing falls back to straight ahead. Homing (`EftAim_Home`): target = opponent node
  0x11 + opponent frame movement x min(distance / speed, 15) x 0.5, turn capped per frame; no
  turn when the target is behind or lock-on is lost.
- No random draws anywhere in 0x12DD80..0x132290.

Hazards and order dependence (verified):
- **The list wraps destructively**: a 65th record in a frame resets the count to 0, dropping
  every earlier record of that frame. Which survive depends on task update order.
- `EftHit_GetNew` does not reserve its slot.
- Record index = creation order, and every pass walks it in order; when several records share
  a task the last one's result wins.
- `EftHit_Clash(a, b)` is not symmetric: the outcome can depend on which record is `a`.
- Homing leads the target with the opponent's frame movement, so it depends on whether the
  opponent has already moved this frame.
- While `BtlScene_IsTimeStopped()` (pause, or stage not ready) no record is added **and the
  list is not emptied**, so the last frame's records stay.

The hit detection that consumes the list is at 0x1AFDB0..0x1B10F0 (not decompiled yet).

Corrections to earlier notes: `BtlBlastRec.active` (+0xC) in btl_scene.h is the record type;
what btl_scene.c calls a record's "definition flags" is the owning task's event flags (so in
`BtlScene_CheckStageChange` the test means "the task hit a fighter").

## Technique effects (simulation; `eft_i.c`, verified unless marked)

- A technique effect type is a pair of task classes in `gEftShotClass` (0x2C3700, row = type +
  1): a group class and an instance class. Two types are in this file.
- Both are driven by the fighter's animation effect events, read with
  `EftShot_TestBits(objId, bit)`: 2 aim, 4 fire, 8 stop, 0x400 end now. Nothing runs while the
  character is stopped (`BtlScene_IsCharStopped`).
- **Type 3, sweeping beam**: the far end moves on a circle around the origin at about the
  opponent's distance (capped at 800), 1.08 degrees per frame, starting 11 steps before the
  opponent. While fired it publishes one hit record per frame (two spheres or two boxes). Each
  frame the beam segment is traced against the stage; on a hit the end is pulled back to the
  surface, a 10-frame impact mark is queued (15 at most; a 16th is dropped) and a hit stage
  object is destroyed.
- **Type 7, follow effect**: keeps a two-sphere hit record on fighter node 3 from the fire
  event until it ends.
- A hit result in the task flags stops the record. At their end both set the fighter's held
  flag 0xA8 ("beam over").
- The effect pack library (`EftEmit_*`) binds six node slots to model nodes the first time each
  event bit is seen and samples them every frame; event 0x400 freezes them.
- Order: `BtlStage_DestroyObj` is called from effect updates, so its order relative to the
  other fighter's effects is the task list order.

## Stage (from stg_b.c, verified)

- `BtlStage_Update` (0x243568): skipped while stage load jobs run; otherwise stage timers,
  stage objects (`BtlStage_UpdateObjs`), the ambience handler, then 0x115370 and 0x2309A8.
- Stage timers add 0.13333333 per unpaused frame and raise a one-frame flag when they pass
  their period (readers not found yet).
- Stage flag word: bits 1 / 2 / 4 enable three stage effect resources; bit 0x10 is what the
  transformation check tests (inferred: the stage has a moon).
- Stage change target comes from the stage info (1 gives stage 0xF, 2 gives stage 3).
- The collision and ground queries the fighters, camera and AI call are in the stg_a range
  (0x23FE70, 0x23FEB0, 0x23FEF8, 0x23FF38, 0x2427A0, 0x242668), not here.

## Blast and beam items (simulation; `eft_j.c`, verified)

`gEftShotClass` rows are {manager class, item class}; row = effect type + 1. Types so far:
0 blast / beam, 2 multiple blast objects, 3 sweeping beam, 5 multi-piece, 6 shot with a model,
7 follow effect.

- Item life (types 0 and 6): START event aims; FIRE puts the head at the muzzle node with
  velocity = aim direction x speed; each flying frame tail = head, optional homing
  (`EftAim_Home`), head += velocity, or head = the position the hit pass corrected; END or
  ABORT (0x400) starts the ending; the task dies after the emitter set's end frames.
- One hit record per frame while fired and not ending: two spheres at head and tail, or two
  boxes from the muzzle to head and to tail. **Radius = item radius x the emitter set's trail
  width animation**, so that "visual" animation track is simulation input and
  `EftEmit_UpdateTrailWidth` cannot be skipped.
- Blast size follows the fighter's charge (fighter +0xE44, and +0xE5C / +0xE60 for two specific
  techniques), which therefore scales the hit shapes.
- The hit pass reports back through the owning task: head corrected, stop, hit a fighter.
- Every update is gated by `BtlScene_IsCharStopped(objId)`.
- Effect code creates battle objects (`BtlObj_Create`) for shot models, consuming object ids.
- Stage blur is driven by effects (definition flag 0x200): visual.
- (verified, eft_b) Original bug: `EftBubble_RandDir` takes its output vector by value, so the
  burst bubbles' direction is uninitialised stack data.
- (verified, eft_b) The texture set effect modules load is 32 entries of 0x10 bytes
  `{u64 tex0; image pointer; pad}` plus a count at +0x200.
- (verified, eft_e) The stage-change transition counts 150 unpaused frames; the stage swap in
  battle_load.c waits on `EftBurst_IsBusy`. Its particles and sounds use libc `rand()`.
- (verified, eft_e) On water stages `EftWater_UpdateBlast` is called for every hit record each
  frame and draws `rand()` once per eligible record before testing anything, so the libc call
  count follows the number of live projectiles.
- (evidence) Source file boundaries: 0x13C300..0x13F3D8 (transition; `EftBurst_Update` needs
  three functions of eft_d.c in the same file) and 0x140338.. through eft_f (water).
- (prelude, to verify) `EftBurst_Update` also needs the HI/LO hazard handled: the agent got a
  match with `.set mips64` instead of `.set mips4` in `__gp_forget` on a private prelude copy;
  it did not test other files with that change.
