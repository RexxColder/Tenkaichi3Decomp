# Battle HUD

Source (linked): src/battle/hud_a.c (manager, 0x2187E0..0x219EB0), hud_a_b.c (team
panel, ..0x21BCA0), hud_a_c.c (caption, ..0x21C0E0), hud_a_d.c (head of the health / ki gauge
part, ..0x21CA60). Layouts in include/battle/hud_a.h, names in config/symbols/hud_a.txt. All 56
functions match. Suggested final names: hud.c, hud_team.c, hud_caption.c; hud_a_d.c belongs to
the gauge file that continues at 0x21CA60. The file split is by module, not proven object
boundaries (the four files concatenated also match). Linked layout: each file defines its work
pointer (`gHud` 0x2FEB3C, `gHudTeam` 0x2FEB40, `gHudCaption` 0x2FEB44, `gHudGauge` 0x2FEB48, in
hud_a_d.c) as `.sdata`; hud_b.c's `.sdata` is the 8-byte colour initialiser of
`HudGauge_UpdateStatIcons` at 0x2FEB50; `.rodata` 0x2F1B30 (hud_a.c) and 0x2F1B90 (hud_b.c);
`.lit4` 0x2FE1D8..0x2FE20C.

## Structure (verified by matching C)

- The HUD is a tree of `HudNode` (0x38 bytes: flags, rotation, x / y, offset, sprite list,
  child list, update callback, draw callback). `HudNode_Update` runs a node then its children;
  `HudNode_Draw` pushes translate and rotate-Z on the VU0 matrix stack, draws, recurses, pops.
- Manager `gHud` (0x38 bytes, heap 2): seven part roots (gauge, timer, team, notice, combo,
  prompt, caption), five sprite sheets, visibility bits at +0x30 (1 gauges and team panel,
  2 timer, 4 notice, 8 combo, 0x10 prompts; set by `Hud_Show*` from the battle sequence),
  replay HUD mode at +0x34.
- One set of nodes serves both players: side 1 is the same tree drawn mirrored (x' = 511 - x),
  with face sprites mirrored back.
- `Hud_Init` reads one common file at `gCommonRes + 0x28`: header words 1, 4, 5, 2, 3 are the
  byte offsets of sheets 0..4; a sheet has a count at +0 and 0x40-byte texture entries at +0x10.

## Link to the simulation

- `Hud_PreUpdate` (called from `BtlGame_PreUpdate`) is the ONLY place the HUD reads the
  simulation, through the `BtlCtrl_*` / `BtlSide_*` queries: health, ki, blast stock, max power,
  stat modifiers, combo hits and damage, clash count, five event messages, button prompt, and
  the team panel's values (reserve count, switch gauge, target health). The node update itself
  runs inside `Hud_Draw` (`BtlGame_Draw`).
- The HUD writes nothing back to the simulation, draws no random numbers, and reads no pad or
  camera. (inferred) For netplay the whole range is presentation.
- Animation uses `Ramp`s stepped once per drawn frame and frozen by `BATTLE_FLAG_PAUSE`
  (the caption pulse is the exception: it runs while paused).

## Values

- Health bars: 10000 health per bar, 160 pixels wide; minimum visible width 3 on the last bar;
  bar colour index 5 / 1 / 4 / 3 for 7 or more / 2 or more / 1 / 0 bars underneath.
- Switch gauge full at 100000; team panel health bar offset = 30 * hp / max.
- Team panel root = (12, 66) + (-256, -224) * (slide + hide): `Hud_SlideOut / In` slide it off
  screen (not a fade, as an older comment in battle_load.c says). Flags 0xE4 / 0xE5 start a
  0.25 s face flip.
- Battle mode 1 draws no team panel; mode 7 draws only the caption; mode 3 shows a count in
  place of the timer. During a replay `gBtlReplayHudMode` picks 0 = caption only, 1 = full HUD,
  2 = nothing; parts are updated in all three.

## Original quirks

`HudCaption_Reset` has no caller; `HudTeam_Reset` sets the target member to 0, not -1; the
two-icon prompt loop would overflow its 4-entry arrays if a query returned more than 2 icons;
health divides by the maximum with no zero check.

## Inferred / guessed

"notice" as the meaning of the fourth part; the caption textures' wording; that the other
parts slide like the team panel; 0x21FA38 being a gauge shake. Guessed names are marked in the
symbol file.

## Gauge part (0x21CA60..0x222400; src/battle/hud_b.c, continues hud_a_d.c; names in config/symbols/hud_b.txt)

31 of 32 functions match; `HudGauge_UpdateAura` is INCLUDE_ASM with a behaviourally exact
attempt (loop shape). Work `gHudGauge` (0x288 bytes; field table in include/battle/hud_b.h),
12 nodes over 76 sprites built by `HudGauge_Init`. Final file: hud_gauge.c merged with
hud_a_d.c.

Verified by matching C:
- Units: health 10000 per bar; ki 20000 per bar, five bars; powered-up timer 6000 per bar,
  five bars; blast 100000 per stock, digit clamped to 7.
- Trailing health falls 100 per frame (at least 120 / 150 / 200 once the gap exceeds 5000 /
  10000 / 20000); trailing ki 400 per frame (606 / 800 for gaps over 20000 / 40000).
- Low health flash while health <= 10000; all five ki lamps pulse at ki > 99999; the blast
  digit pulses while blast >= `BtlSide_GetBlastMax`.
- Status icons: four, 24 px apart, 0.2 s fade-out, 0.3 s slide (1 - (1 - t)^3), 0.2 s fade-in.
- **Correction applied to hud_a.h**: `gHudGauge + 0x30` is health now (`cur.hp`) and `+0x28` the
  trailing value (`hpShown`); the header had them reversed; 0x21FA38 / 0x21FA88 are `HudGauge_ShakeHp` / `ShakeKi(u16
  count, u16 amp)`.

**Random draws (netplay hazard)**: the HUD draws from libc `rand()` through `Rand_IntRange`:
two per frame per shaking node (`HudGauge_UpdateHpTrail`), and at least 30 per side per frame
in `HudGauge_UpdateAura`, whether or not the aura is visible, none while paused. These are
visual, but libc `rand()` also reaches the simulation (docs/netplay_notes.md), so the HUD
advances a stream the fight reads. The earlier statement "the HUD draws no random numbers"
holds only for 0x2187E0..0x21CA60. A port must give the HUD its own generator, and a faithful
replay of a PS2 recording must reproduce these draws.

Original quirks: sprite 62 is both the tenth aura spark and the blue "powered up" flash, so
only nine sparks show; `kiReserve / 20000` is computed and discarded; the ki reserve ramp
steps while paused; the bar-count update runs from a draw callback.

Inferred: status icon bits 0..3 = stat modifiers 0, 3, 1, 2; `kiReserve` is a pulsing red
layer under the ki bars of unknown meaning; node 10 is the powered-up aura.

## Notice part: announcements (0x226488..0x22A750; src/battle/hud_d.c, hud_d_b.c, not linked yet; names in config/symbols/hud_d.txt)

All 28 functions match. hud_d.c (6 functions) is the tail of the HUD sprite / node library
(`HudSprite_Draw`, `HudNode_Show / SetPos / SetOfs / SetRot`; `HudNode` +8 / +0xC are two
floats, not `s32 unk8[2]`). hud_d_b.c is the head of the notice module that continues in
hud_e.c; final name hud_notice.c.

- (verified) `gHudNotice` (0x2FEB5C, 0x74 bytes; sheet 1, 16 sprites, 4 nodes) shows one
  announcement at a time. `HudNotice_Show(id)` installs an update / draw pair on node 1 at
  (256, 224); each update is a state machine on two `Ramp`s. Ids by caller: 0
  `BtlSeqReady_Update`, 1 `BtlSeqReady_Exit`, 2 K.O., 3 K.O. with
  `BattleResult_IsWinnerEvent59Clear`, 4 `BattleResult_IsReasonBit2`, 5 time up, 6 winner
  scene, 7 banner (hud_e.c), 8 from `Hud_PreUpdate` on `BtlCtrl_TestProgressFrameBit`. The
  wording (READY / FIGHT / K.O. ...) is inferred. Timings are in the source comments.
- (verified) **Random draws**: `Rand_IntRange(-16, 16)` (libc `rand()`) once per HUD update
  while the streak shakes (announcements 1, 2 and 6, for 0.45 / 0.55 / 0.8 s). Visual, but it
  advances the stream the simulation reads; same hazard as the gauge part.
- (verified) None of the announcement updates checks `BATTLE_FLAG_PAUSE`.
- Original quirks: the 0.6 s hold of announcements 3 / 8 never happens (the wrong ramp is
  stepped); in the winner announcement copy 1 is always hidden.
