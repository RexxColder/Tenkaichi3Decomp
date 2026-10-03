# Current status (handoff note)

Written 2026-10-04 so the work can be picked up without the conversation that produced it.
Update or delete when it goes stale.

## Verified state

- Last build verified byte-identical: the commit "Link the fighter core" (21.07% of the main
  executable's game code in C; 77 C files linked; 2079 functions diff clean; DBZP 0%).
- Check at any time: `.venv/bin/python configure.py && ninja`, then
  `cmp build/SLUS_216.78.rom disc/SLUS_216.78.rom` and `cmp build/DBZP.BIN disc/BIN/DBZP.BIN`,
  then `python3 scripts/progress.py`.

## Fighter-core batch: linked

Script commands, stats and animation, hits and collision, members, input conditions and
control, flags and clashes, movement, the action core, and the AI's virtual pad, sense and
rule evaluator are all linked. Merges made at integration: `btl_char_coll.c` into
`btl_char_hit.c`; `btl_char_ctl.c` into `btl_input.c`; `btl_ai_think.c` into `btl_ai_cond.c`
(`BtlAi_ScaleByLevel` moved to the end of `btl_ai_seq.c`); `BtlColl_NextPoolMember` moved to
the top of `btl_char_member.c`. Seven functions of that batch stay INCLUDE_ASM
(`BtlAiSense_IsBusy` and six `AiThink_*`).

Still in the tree but NOT linked or committed:

| Files (src/battle/) | Range | Notes |
|---|---|---|
| btl_char_fx.c, _b.c, _c.c | 0x1D00D8..0x1D3B40 | 64/67 match per function. `BtlFx_UpdateGroundFx` needs the fx-bit helpers of btl_char_member.c in the same file. The C emits .lit4 at 0x2FD1D8.. with two assembly-owned gaps (see the agent notes in combat.md) |
| btl_act_a.c | 0x1E3158..0x1E6CC0 | 21/21; continues btl_char_action.c's object; 16 jump tables 0x2EF370..0x2EF880, .lit4 0x2FD4DC..0x2FD668, **.sdata 8 bytes at 0x2FEB20** |
| btl_act_b.c | 0x1E6CC0..0x1EA5F8 | 21/23; `BtlAct_AttackDashHandler` matches with `BtlAct_RequestAttackEnd` (btl_act_a.c) in the same TU: **merge btl_act_a + btl_act_b** (and they continue btl_char_action.c's object); `BtlActB_TickMemberChange` 14 instructions off. Jump tables 0x2EF880..0x2EF91C, .lit4 0x2FD668..0x2FD7C8 |
| btl_act_c.c | 0x1EA5F8..0x1EE058 | 29/30 (`BtlAct_GuardHandler` INCLUDE_ASM, 2 instructions); jump tables 0x2EF920..0x2EFA34, .lit4 0x2FD7C8..0x2FD988 with five LIT4_WORD entries |
| btl_act_d.c | 0x1EE058..0x1F1930 | 18/18; 2 jump tables at 0x2EFA40, .lit4 0x2FD988..0x2FDB44. `BtlAct_DashMoveHandler` (0xF) vs btl_act_e's `BtlAct_DashHandler` (0x1A): no clash now |
| btl_act_e.c | 0x1F1930..0x1F5460 | 20/20; 5 jump tables 0x2EFB20..0x2EFC80, .lit4 0x2FDB44..0x2FDD08. 0x1A is `BtlAct_DragonDashHandler`; a comment in btl_act_d.txt about the old name is stale |
| btl_act_f.c | 0x1F5460..0x1F8C00 | 25/25; 11 jump tables 0x2EFC80..0x2F0780, .lit4 0x2FDD08..0x2FDDAC; a slice of a larger object |
| btl_capi_a.c | 0x204E78..0x207020 | 79/81 (`BtlCharApi_HasKiBlastType2/3` INCLUDE_ASM); .lit4 0x2FE07C..0x2FE0C8; no rodata |
| btl_capi_b.c | 0x208430..0x20BA80 | 136/136; jump table 0x2F1600, .lit4 0x2FE0CC..0x2FE0E4 (btl_char_api.c's comment naming 0x2FE0D4 as the pool end is wrong) |
| btl_tech_a.c | 0x20BA80..0x20F0E8 | 124/124; rodata 0x2F1620..0x2F18F0 (3 jump tables + three f32[10] tables), .lit4 0x2FE0E4..0x2FE114. 19 names changed mid-run: check other files for stale ones at link time (list in the agent notes of combat.md / symbol file) |
| btl_tech_b.c | 0x20F0E8..0x2129C8 | 182/182; 3 jump tables 0x2F18F0..0x2F19CC, .lit4 0x2FE114..0x2FE1C0. Names ~80 functions other files call as `func_`: run apply_names after listing its symbol file |
| btl_act_g.c | 0x1F8C00..0x1FC2B0 | 13/14 (`BtlAct_SuperRushDashHandler` INCLUDE_ASM, owns .lit4 0x2FDDAC..0x2FDDEC and jump tables 0x2F0780, 0x2F08B0); C .lit4 0x2FDDEC..0x2FDEB0, rodata to 0x2F0FCC. `BtlAct_SuperRushFollowHandler` dispatch is fragile: re-check after any header change |
| btl_act_h.c, btl_act_h_b.c | 0x1FC2B0..0x1FC598, ..0x1FFAC0 | 20/21 (`BtlAct_GrabDash` INCLUDE_ASM owning .lit4 0x2FDEB4..0x2FDEBC); .lit4 0x2FDEB0 and 0x2FDEBC..0x2FE008; no rodata |
| btl_act_i.c | 0x1FFAC0..0x203168 | 16/18; `BtlAct_SwitchArriveLand` and `BtlAct_KoSwitchFlyIn` match only with `BtlActChange_SetFlags` (0x1FD958) and `BtlActChange_Finish` (0x1FDF50) in the same TU: **merge with btl_act_h.c**. Jump tables 0x2F0FD0..0x2F11C8, .lit4 0x2FE008..0x2FE070 |
| btl_act_j.c | 0x203168..0x204E78 | 28/28; 9 jump tables 0x2F11D0..0x2F15F8, .lit4 0x2FE070..0x2FE078 |

`scripts/apply_names.py` reads every file in config/symbols/, including those of agents whose
files are not in the yamls yet: run it only after listing them, or with their stems as skip
arguments.

## Running when this was written

All fourteen decomp agents of the action-handler batch have reported (rows above). An
integration agent is linking the batch plus btl_char_fx*.c. Evidence of original file
boundaries to apply: btl_char_action.c + btl_act_a + btl_act_b are one object; btl_act_f +
btl_act_g continue each other; btl_act_h_b (from 0x1FD958) + btl_act_i are one object;
btl_act_g's last two functions (from 0x1FC008) probably belong with btl_act_h.

## Known follow-ups

- Apply the btl_tech_a rename list (`BtlParam_GetRateA` -> `GetKiChargeRate`, `GetStepA` ->
  `GetMaxPowerGain`, `GetSlotId` -> `GetTransformTarget`, `BtlStat_GetRate0..3` / `GetScale11/12`
  -> ki charge / regen / recover / blast gain bonuses, max power charge / extra time, ...).
- `btl_char_hit.h` describes the attack table as inline at object +0x920; it is a pointer.
- Apply the `BtlCtrl_*` / `BtlFacade_*` renames proposed by the btl_capi_b agent (PlayMotion,
  IsMotionPlaying, Transform, Fuse, UseTechnique, SetMaxPower, SetAuraOn/Off, Ki / Blast names);
  fix the "ratio" comments in btl_facade.c (the value is a percentage).
- Rename the numbered stat curves (`BtlStat_GetScale4..10`) to the meanings in combat.md.
- Check whether `BtlFx_SpawnDamageSparks` is really the ki blast launcher (it draws `BtlChar_RandF`).
- The previous action (fighter +0x950) has no writer anywhere (searched the whole executable);
  see combat.md. Fix the comment-level claims in handlers that assume it works.
- Rename `BtlAct_IsDamageId` (it is "is a technique action id") and fix the parameter order of
  `BtlAnim_AdvanceThen` in btl_char_status.h to `(chr, next, blend, flags)`.
- One unified fighter header: every battle file has its own partial view of the 0x1600-byte
  fighter object; the merged picture is in docs/systems/fighter.md and combat.md.
- Names proposed by the script-command agent for `BtlFacade_*` placeholders (lip sync, ki and
  blast gauge adders, CPU level; `BtlFacade_IsCharMoveDone` is inverted) are not applied yet.
- `Snd_SendFighters` sends sound handles, not fighter ids; `ADXF_Tell` in
  config/symbol_addrs.txt may be the inner unlocked function. Neither is fixed yet.
- 13 functions in linked files are still INCLUDE_ASM (docs/open_questions.md).
- `BtlAi_GetPairRate` / `BtlAi_GetQuadRate` (btl_ai_cond.c): the switch shape that matched
  `AiThink_GetSubRate` may fix them.

## After this batch (docs/roadmap.md)

Effect tasks 0x12DD80..0x1AE200 and the stage 0x23FB20..0x248F28 complete the simulation; then
the headless PC simulation validated against the game's replay format.
