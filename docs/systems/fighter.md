# Fighters

Sources: `src/battle/btl_char_mgr.c` (roster, reset, per-frame phases), `btl_char_get.c`
(accessors, fighter generator, vibration, voice), `btl_char_api.c` (by-object-id interface),
`btl_input.c` (input, see input.md), `btl_replay.c`, and the fighter camera in
`btl_char_cam.c`, `btl_char_cam_cut.c` and `btl_char_cam_modes.c` (one module, three files
because the unmatched cut evaluator owns constants in the middle of the float pool).

Each file declares its own partial view of the fighter object (`BtlMgrChr`, `BtlCharGetChr`,
`BtlCharApiChr`, `BtlInputChr`, `ChrCamChr`, ...). A single unified header does not exist yet;
the table below is the merged picture.

The action state machine, movement, hit detection, gauges and effects are NOT decompiled: the
phase order below is verified, but what each still-unnamed callee does is inferred.

## Roster `gBtlChars` (0x280 bytes, heap; verified)

| Offset | Field |
|---|---|
| 0x00 | count (2 in a battle) |
| 0x04 | fighters: count x 0x1600 bytes |
| 0x08, 0x0C | two per-side arrays of 0x34 bytes; +0xC holds four live sound handles per side |
| 0x10 | flags, bit 0 = fight started |
| 0x14 | frame counter (30 bits), ticked only when started and not time-stopped |
| 0x18 | fighter random generator state |
| 0x1C | stage timer, counts down |
| 0x20..0x34 | six table pointers into common file 2; `tbl[0]` is indexed by fighter +0x974 |
| 0x24 | (tbl[1]) the common camera cut table |
| 0x28 | (tbl[2]) the voice table `{s16 first, s16 count, f32 interval}` |
| 0xA0 | four positional sound requests, count at +0x120 |
| 0x130, 0x134 | replay viewer row (always 0) and watched side |
| 0x274 | "time stopped" word: freezes both simple generators, the frame counter and input; writer not found |

## Fighter object (0x1600 bytes)

Offsets verified by matching code in at least one file; meanings as marked.

| Offset | Type | Meaning | Status |
|---|---|---|---|
| 0x0000 | s32 | player: roster index, replay track index, key-config index | verified |
| 0x0004 | s32 | pad index | verified |
| 0x0008 | s32 | side | verified |
| 0x000C | s32 | battle object id | verified |
| 0x0010 | 0xF0 bytes | pose block; world position at +0x10, facing yaw at +0xA4 | verified |
| 0x0100 | 0xF0 bytes | last frame's pose | verified |
| 0x0420 | 0xA0 bytes | fighter camera (`ChrCam`): eye, pos +0x430, rot +0x440, target, shake +0x470, trace result, distMode +0x49C, **yaw +0x4A0**, side +0x4A8 | verified offsets |
| 0x04C0 | 0xB0 bytes | camera cut state | verified offsets |
| 0x0570 | 0x3D8 bytes | input block (`BtlCharInput`): raw sample, mask table +0x578, held/pressed/released +0x73C, ring +0x8C0, record +0x938 | verified |
| 0x0948 / 0x0950 | s32 | action id / previous action | verified |
| 0x0974 | s32 | state id indexing roster `tbl[0]` | verified use, meaning unknown |
| 0x0994 / 0x0998 | s32 | active member index / member count | verified |
| 0x09A4 | 5 x 0xA4 | member entries (below) | verified |
| 0x0D40, 0x0D44 | s32 | running maxima copied to the result block | verified |
| 0x0E5C, 0x0E60 | s32 | counters, full at 3 and 5 | verified |
| 0x0F30 | Vec4 | written by the opponent's post-scene pass | verified |
| 0x0F50 | 4 x 0x18 | status table owned by the next module | inferred |
| 0x1084 | u8 | per-frame stage number (1..12) | inferred |
| 0x1278 | s32 | input is injected (CPU) | verified |
| 0x127C..0x1284 | u32, f32, f32 | injected buttons, stick x, y | verified |
| 0x12D0.. | s32 | chara / costume of the form being changed into | verified |
| 0x1320..0x1328 | s32 | hit-stop: freeze counter, pending length, delay | verified |
| 0x1330 | s32 | a loaded resource to release | verified |
| 0x1370 / 0x1454 | s32[57] | last voice line / voice cooldown per kind | verified |
| 0x1538 | u64 | per-frame bit set | verified |
| 0x15D0..0x15E4 | | vibration: enabled, power, frames, small-motor frames, phase | verified |
| 0x15E8 | 3 pointers | tables inside the object's three files | verified |

Member entry (0xA4): chara, costume, present, `bonus[7]`, `ability[4]`, cpuLevel +0x38,
aiType +0x3C, gauge block at +0x40.

Gauge block: health / health max at +0 / +4 (10000 units per bar), two more gauges with maxima
at +0xC / +0x10 and +0x14 / +0x18 ("ki" and "blast" are guesses), +0x1C = 30000 together with
flag 6, low-health flags, a mode flag at +0x30.

## Lifecycle (verified)

- `BtlChar_AllocAll(2)` from `Battle_Init`; `BtlChar_FreeAll` from `Battle_Term`.
- `BtlChar_ResetAll` (init, restart, and the first Ready/Fight frame): zeroes every fighter and
  the roster's working areas, then per fighter binds the battle object and runs
  `BtlChar_Reset`. **Nothing in a fighter survives a restart.**
- `BtlChar_Reset` copies from the battle setup: member count, each member's chara, costume,
  cpuLevel, aiType, `bonus[1..7]`, `ability[0..3]`, health (through a parameter loader), the
  per-side camera options, whether the side is CPU (`+0x1278`), and the vibration setting from
  the save. It then resets input, places the fighter and rewinds the replay track.
  Not copied here: `BattleMember.items` and `bonus[0]`.

## One simulated frame (call order verified; callee purposes inferred)

Fighters are updated pass by pass, fighter 0 first within each pass. Each per-fighter body
returns immediately while the fighter is frozen (hit-stop).

1. **`BtlChars_CheckStart`** (before the pad read): when the sequence first reaches Ready or
   Fight, reset the whole roster once more (not in mode 1) and set "started".
2. **`BtlChars_SampleInput`** (skipped when paused or loading): `BtlInput_Sample` per fighter.
3. **`BtlChars_UpdateInput`** (skipped when paused): tick the frame counter; round-reset check
   (fighter flag 0xF9); hit-stop; per fighter save last pose, clear per-frame fields, tick
   voice timers, `BtlInput_Update`; set one held flag 1..4 from the sequence state.
4. **`BtlChars_UpdateMain`** (skipped when paused or loading), seven roster passes with global
   steps between: action state machine; push-out between fighters; movement; stage and ground
   queries; camera control; then gauges, hit detection and damage (or a reduced path while
   anyone is frozen); effect spawning.
5. (effect scene runs here, from `Battle_Update`)
6. **`BtlChars_PostScene`**: fighter camera pose (`ChrCam_Update`), low-health flags, and a
   vector written into the *opponent*.
7. **`BtlChars_EndFrame`**: late update, object flag update, queued sounds, then per fighter
   `BtlChar_RaiseEvents` and `BtlChar_UpdateVibration`.

## Hit-stop (verified)

Level per fighter: 2 with flag 0x125, 1 with flag 0x126. If any level is non-zero, every
fighter is scheduled to freeze except those at the highest level (and, at level 1, those with
flag 0x127). At level 2 the loop stops at the first exempt fighter, so roster order matters.
A frozen fighter skips every phase, including input sampling.

## Character changes (verified)

`BtlChars_OnModelLoaded(side)` runs when the character load job finishes, by current action:
- 0xEC..0xF0 transformation: new chara/costume, parameters reloaded, health keeps its ratio.
- 0xF1, 0xF2 fusion: bonuses summed and clamped to -20..40, abilities OR-ed, health and
  maximum summed, the partner marked absent.
- 0xF3..0xF8 member switch.
Parameter byte 0xAD adds +5000 / +10000 health or fills a gauge.

## Events raised per frame (verified)

`BtlChar_RaiseEvents` raises battle events from fighter state: 0x16..0x1C for health lost in
steps of 10000 (only the highest), 0x1D / 0x1E for full gauges, others keyed by action id,
technique id or flag edges. The full table is in the header comment of `btl_char_mgr.c`.
Under the fight state it also keeps `BattleResult.health[side]` as a team health percentage.

## Random numbers used by fighters (verified)

- `BtlChar_Rand`: roster+0x18 = `(state * 714025 + 4096) % 150889`, zeroed by `ResetAll`.
- `BtlChar_FrameMod(n)`: frame counter % n, used as a deterministic pick (39 call sites).
Both are frozen while roster+0x274 is set.

## Voice (verified)

57 voice kinds per fighter with a per-kind cooldown; the line is picked with `FrameMod`,
avoiding the last one. In mode 1, kinds 14..16 and 34 are suppressed.

## Fighter camera (matches per function, not linked; names are guesses)

`ChrCam` at fighter +0x420. Two phases per frame: control (cut start, demo triggers, yaw and
input) from the main update, and pose from the post-scene pass.

- Pose priority: frozen (flag 0xCC) > cut > fixed (0xDC/0xB8) > lock-on (flag 5) > free. Then
  shake, clamp to the stage radius, smoothing, keep-behind-head, stage trace.
- SELECT cycles three distance presets. In lock-on the yaw follows the opponent at 25% per
  frame and the right stick picks the side the camera sits on.
- **Movement is relative to `yaw` (+0x4A0)**, which depends only on opponent direction, fighter
  flags, the fighter's input record and facing. (Writers verified; the movement-side readers
  were read from disassembly.)
- No pad is read; all input comes from the fighter's input block.
- Cuts are 0x24-byte records (per character at object +0x938, common at roster +0x24) that
  orbit one model node and look at another; requested from 31 places in the fighter code.
- Non-simulation inputs: `Battle_IsSplitScreen()` changes the lock-on pose, and a per-side
  option gates camera shake (and therefore `rand()` calls). See netplay_notes.md.

## Fighter flags seen (numbers verified, meanings inferred)

1..4 sequence state; 5 lock-on / basic controlled state; 6 a powered-up mode; 0xD3 this
fighter's camera takes the full screen; 0x125..0x127 hit-stop; 0x128 holds off the
end-of-battle check; 0xF9 round-reset request; 0x135 time stopped; 0xAB hit by a finishing
technique. Longer lists are in the reports summarised in the headers.

## Scope note

`btl_char_api.c` and `btl_char_get.c` are slices of larger original files (about 330 functions
from 0x204E78, and the object around 0x1DBAD0..0x1DCB88). They link on their own only because
they own no shared constants.

## Corrections and additions from the fighter-core batch

See combat.md for the mechanics. Corrections to the tables above (all verified by matching C):

- +0x0974 / +0x0978 / +0x097C / +0x0980 are the current / requested / previous / second-layer
  **animation id**, not a "state id"; roster `tbl[0]` is the per-animation flag word table.
- +0x0948 is the action state `{current, request, prev, queue[4]}`; +0x094C is the requested
  action (-1 = none), and nothing ever writes `prev` at +0x0950.
- +0x00A4 is the **heading** yaw; the model yaw is the rotation y at +0x0024 and follows it.
- +0x0D40 / +0x0D44 are the combo damage total and combo hit count; +0x0D48 the combo timer.
- +0x0F40 / +0x0F44 / +0x0F48 are hit status (pending target, last hit number, contact
  counter); +0x0F50 is the stat modifier table (4 x 0x18).
- +0x0FB0 is the reaction block filled by the hit code (reaction id, push yaw, launch angles,
  stun at +0xFE0, deferred damage at +0xFEC).
- +0x1085..+0x1261 are the flag arrays and stamps; +0x1262 / +0x126B are 72 one-frame effect
  request bits and last frame's copy.
- +0x02E0 / +0x0350 / +0x03C0 are per-pass position and rotation snapshots and their mask.
- +0x0918 holds two per-frame bit sets of the input conditions queried this frame.
- +0x15A0 is the head-tracking state.
- Roster +0x08 / +0x0C are the per-side one-shot and looping sound slot sets; +0x40 is the
  clash state machine; +0x138..+0x278 is the character-change queue; **+0x274 is its
  "time stopped" word, written by `BtlChange_Update`** (the writer earlier listed as not found).
- The manager's step named `BtlChars_UpdateCollision` calls `BtlColl_Update`; the step labelled
  "push-out" in the phase list is a hold attachment for grabs (`BtlChars_UpdateHold`), and the
  real push-out is `BtlMove_PushOut` in the stage-7 pass. The step labelled "gauges" only
  applies queued hits and drains (`BtlMembers_UpdateQueuedDamage`); gauges are updated by
  `BtlAct_UpdateGauges` inside each fighter's action update.

## Fighter API, second part (`btl_capi_b.c`, 0x208430..0x20BA80)

All 136 functions match per function; not linked yet. Full tables are in
`include/battle/btl_capi_b.h`. Four groups (verified code; names partly guessed):

- **By object id** (66 functions, used by the AI): gauges, current action, whether the fighter
  can transform / fuse / switch, the opponent's current technique kind and class, clash
  counters, stun timer, an armour level, and the incoming-blast test. A blast counts as
  incoming when it is not moving away and `distance - body radius - 10 <= last step * 5`
  (1.9 for technique records); each is reported once (blast record +0x180).
- **By player index, `BtlCtrl_*`** (43 functions, used by the battle sequence and the story
  script): entrance / win / lose poses (one-frame flags 0xEF..0xF2), scripted motions, warps,
  aura and charge effects, hide / show, gauge edits, forced transformation / fusion / switch /
  technique, `BtlCtrl_CanAct`, `BtlCtrl_IsInterruptible`.
- **Loader side**: wrappers the model loader uses to drive the character-change queue.
- **By side, `BtlSide_*`** (used by the HUD, inferred): gauges, switch target, combo read-out
  (taken from the opponent's combo counters at +0xD40).

Script units (verified): health in percent of the member's maximum; ki in units of 20000;
blast in stocks of 100000. "Raise" means at least, "Lower" at most. Every forced action clears
flag 0xBE and zeroes the stun countdown.

`BtlCtrl_IsMoveDone` is confirmed inverted: it returns 1 while a one-shot motion is still
playing. A list of proposed renames for the `BtlCtrl_*` and `BtlFacade_*` placeholders
(PlayMotion, Transform, Fuse, UseTechnique, SetMaxPower, aura and charge effects, Ki / Blast
gauge names) is in the agent's symbol file comments and docs/status.md follow-ups.

The code assumes a fighter's object id equals its side / player index.

Random draws: two `Rand_Range` users (`PickFusionSlot`, `PickTransformSlot`), both without a
caller in either binary. No pad, camera or sound reads; nothing depends on player 0.
