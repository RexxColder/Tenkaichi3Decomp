# Current status (handoff note)

Written 2026-10-04 so the work can be picked up without the conversation that produced it.
Update or delete when it goes stale.

## Verified state

- Last build verified byte-identical: the commit "Link the action handlers" (34.54% of the main
  executable's game code in C; 91 C files linked; 2887 functions diff clean; DBZP 0%).
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

## Action-handler batch: linked

The fighter effect layer, the action handlers, the fighter API and the technique / parameter readers
(0x1CF578..0x1D3B40 and 0x1E3158..0x2129C8, all of it) are linked and the build is byte-identical:
91 C files, 2887 functions diff clean, 34.54% of the main executable's game code in C.

Files as linked (src/battle/), with the merges made at integration:

| File | Range | Notes |
|---|---|---|
| btl_char_member.c | 0x1CDCA8..0x1CF578 | lost its tail (the effect request bits) to btl_char_fx.c |
| btl_char_fx.c | 0x1CF578..0x1D1EC8 | the tail of btl_char_member.c + btl_char_fx.c + btl_char_fx_b.c: `BtlFx_UpdateGroundFx` needs `BtlChar_IsFxBitNew` defined in its file and now matches in C. `BtlFx_SpawnSpeedLines` (four LIT4_WORD) and `BtlFx_SpawnDamageSparks` (last function, RODATA_ALIGN16; its constants 0x2FD200..0x2FD214 are the assembly chunk `cod/1FD200`) stay INCLUDE_ASM |
| btl_char_fx_c.c | 0x1D1EC8..0x1D3B40 | unchanged |
| btl_act_a.c | 0x1E3158..0x1EA5F8 | btl_act_a.c + btl_act_b.c: `BtlAct_AttackDashHandler` now matches in C. `BtlActB_TickMemberChange` stays INCLUDE_ASM. Emits the 8 bytes of `.sdata` at 0x2FEB20. Not merged into btl_char_action.c (nothing needs it; it would be a third fighter view in one file) |
| btl_act_c.c, _d.c, _e.c | 0x1EA5F8.., 0x1EE058.., 0x1F1930..0x1F5460 | as written; `BtlAct_GuardHandler` INCLUDE_ASM |
| btl_act_f.c | 0x1F5460..0x1FC2B0 | btl_act_f.c + btl_act_g.c: `BtlAct_SuperRushDashHandler` matches in C once it is in one file with the first part (found at integration). `BtlAct_SuperRushFollowHandler` carries a redundant prototype as a matching aid (see the comment there and decomp_guide.md) |
| btl_act_h.c | 0x1FC2B0..0x1FC598 | `BtlAct_GrabDash` INCLUDE_ASM; its constants are the assembly chunk `cod/1FDEB4` |
| btl_act_h_b.c | 0x1FC598..0x203168 | btl_act_h_b.c + btl_act_i.c: `BtlAct_SwitchArriveLand` and `BtlAct_KoSwitchFlyIn` now match in C |
| btl_act_j.c | 0x203168..0x204E78 | as written; its float pool is 0x2FE070..0x2FE07C (three constants, not two) |
| btl_capi_a.c | 0x204E78..0x207020 | `BtlCharApi_HasKiBlastType2/3` INCLUDE_ASM; float pool 0x2FE07C..0x2FE0CC |
| btl_capi_b.c | 0x208430..0x20BA80 | as written |
| btl_tech_a.c, btl_tech_b.c | 0x20BA80..0x20F0E8..0x2129C8 | as written |

In a merged file each part keeps its own header and its own view of the fighter; functions the first part
already declared are reached from the second part through cast macros
(`#define Name ((ret (*)(args))Name)`), which leave the generated code unchanged.

Seven functions of this batch stay INCLUDE_ASM (docs/open_questions.md).

Evidence of original file boundaries not acted on: btl_char_action.c + btl_act_a.c are one object (the
float pool and jump tables run on); btl_act_f.c's last two functions (from 0x1FC008) probably belong with
btl_act_h.c; where the object holding btl_char_fx.c starts (0x1CF578 is the latest possible place).

`scripts/apply_names.py` reads every file in config/symbols/, including those of agents whose
files are not in the yamls yet, and its skip arguments only skip the source files it rewrites, not the
symbol files it reads: while unlisted symbol files exist, run a copy that ignores them.

## Effect and stage batch (seventh), brief in docs/briefs_effects_stage.md

Nothing of this batch is linked or committed yet (untracked `eft_*` / `stg_*` files). Findings
are recorded in docs/systems/effects_stage.md as reports arrive; each file is re-diffed with
`python3 build/scratch_verify/v.py <stem> keep` (works on a scratch copy when the file has
INCLUDE_ASM).

First wave (stems `eft_a` .. `eft_m` over 0x12DD80..0x1637A0 with cuts at 0x132290, 0x136760,
0x13A9D0, 0x13EA00, 0x142CA0, 0x147050, 0x14B108, 0x14F230, 0x1532A0, 0x157398, 0x15B550,
0x15F728; `stg_a` 0x23FB20..0x242D28, `stg_b` ..0x245F58, `stg_c` ..0x248F28).
ALL SIXTEEN REPORTED and are documented in docs/systems/effects_stage.md; every file was
re-diffed. Nothing of this batch is linked yet. Merge evidence for the integration: eft_c +
eft_d.c (surfaces, 0x138178..0x13C300); eft_d_b + first part of eft_e (transition,
0x13C300..0x13F430); rest of eft_e + eft_f (water); eft_g tail + eft_h + eft_i (shot manager
and effect pack library); eft_j + eft_k + eft_l.c; eft_l_d + eft_m (speed lines).

Relaunched 2026-10-04 (20 agents, the session limit): one integrator linking eft_a..eft_m and
stg_a..stg_c, and nineteen decomp agents:

| Stem | Range |
|---|---|
| eft_det_a | 0x1AE200..0x1B16F0 (projectile hit detection 0x1AFDB0..0x1B10F0; stage collision query `func_001B14C0`) |
| eft_det_b | 0x1B16F0..0x1B4140 (stage collision, fighter-through-object 0x1B24B8, `func_001B2DF0` segment trace) |
| stg_d | 0x22FD10..0x230B38 (stage rigid bodies) + func_00115370 in stg_d_b.c |
| eft_n | 0x1637A0..0x167E68 (aura, second half) |
| eft_o | 0x167E68..0x16C2E0 (blast object module 0x16A400..) |
| eft_p | 0x16C2E0..0x170A50 (pieces of `EftMulti`) |
| eft_q | 0x170A50..0x174A70 |
| eft_r | 0x174A70..0x178AB0 (ki blasts) |
| eft_s | 0x178AB0..0x17CB40 |
| eft_t | 0x17CB40..0x180BF8 |
| eft_u | 0x180BF8..0x1853C8 |
| eft_v | 0x1853C8..0x1895E8 (impact effect 0x187BE0) |
| eft_w | 0x1895E8..0x18D618 |
| eft_x | 0x18D618..0x191D28 |
| eft_y | 0x191D28..0x195EE8 |
| eft_z | 0x195EE8..0x199F28 (ground impact 0x1975A8) |
| eft_aa | 0x199F28..0x19E0C0 |
| eft_ab | 0x19E0C0..0x1A21A8 |
| eft_ac | 0x1A21A8..0x1A62C8 |

NOT launched yet (no free slot): eft_ad 0x1A62C8..0x1AA7E8 (part kind 12 module 0x1A6598, kill /
alive 0x1A65E8 / 0x1A6B40; 0x1A6C58 shock wave; 0x1A6FD0 fighter request 6; model object helpers
0x1A7608.., `BtlObj_Create` wrapper 0x1A8C40; rotated sprite 0x1AA188), eft_ae 0x1AA7E8..0x1AE200
(task helpers 0x1ADA58 / 0x1ADB78 / 0x1ADB98, texture set 0x1ADC68 / 0x1ADF20 / 0x1AE148 /
0x1AE1F8), and the animation player in the battle object (functions around 0x24D498, 0x24D518,
0x24D610, 0x2500E8, 0x250570, 0x2505A8, 0x250940, 0x250B88; see docs/roadmap.md), plus the
unassigned first half of the AI sequence file (0x1B4140..0x1B6008).

## Known follow-ups

- Prelude: test `.set mips64` in `__gp_forget` (HI/LO hazard after mfhi before mtc1) against
  ALL linked files; needed by `EftBurst_Update` (eft_e).
- Stale extern names once eft_e is listed: battle_load.c `func_0013F310/3A0/3C8` =
  `EftBurst_Start/IsBusy/End`; btl_scene.c `func_00140ED0` = `EftWater_UpdateBlast`;
  btl_char_fx.c `func_00140358/478` = `EftWater_SetWake/AddSplashFor`.
- When eft_g is listed: `func_0014AB90` in the linked btl_char_fx.c / btl_char_member.c becomes
  `EftShot_Request`.
- Stage rigid bodies / destructibles at 0x22FDA0..0x230AA0 have no owner yet: add to the next wave.
- Done at this integration: the `BtlCtrl_*` renames (PlayMotion, StopMotion, IsMotionPlaying, SetRot,
  Transform, Fuse, UseTechnique, SetMaxPower) with their one-to-one `BtlFacade_*` wrappers, the numbered
  stat curves (`BtlStat_GetRate0..3`, `GetScale4..6`, `8..12`), `BtlAct_IsTechniqueId`, and the parameter
  order of `BtlAnim_AdvanceThen` in btl_char_status.h and its definition.
- `BtlAnim_AdvanceThen` is still declared `(chr, next, flags, blend)` in the local externs of
  btl_char_action.c, btl_act_c.c, btl_act_d.c, btl_act_e.c and btl_act_h.h (the registers are the same, so
  it links and matches); converting them means swapping the last two arguments at every call.
- Not renamed: `BtlStat_GetScale7` (second damage-taken multiplier), `BtlFacade_ForceAction01/23/4`,
  `BtlFacade_ForceActions` (they force a reaction on the other side as well, so they are not plain wrappers
  of `BtlCtrl_UseTechnique`), the `SetAuraOn/Off` and Ki / Blast names proposed by the btl_capi_b agent.
  The "ratio" comments in btl_facade.c (the value is a percentage) are not fixed.
- Comments in docs/systems/ still use the old names of the functions renamed here.
- `btl_char_hit.h` describes the attack table as inline at object +0x920; it is a pointer.
- Check whether `BtlFx_SpawnDamageSparks` is really the ki blast launcher (it draws `BtlChar_RandF`).
- The previous action (fighter +0x950) has no writer anywhere (searched the whole executable);
  see combat.md. Fix the comment-level claims in handlers that assume it works.
- One unified fighter header: every battle file has its own partial view of the 0x1600-byte
  fighter object; the merged picture is in docs/systems/fighter.md and combat.md.
- Names proposed by the script-command agent for `BtlFacade_*` placeholders (lip sync, ki and
  blast gauge adders, CPU level) are not applied yet.
- `Snd_SendFighters` sends sound handles, not fighter ids; `ADXF_Tell` in
  config/symbol_addrs.txt may be the inner unlocked function. Neither is fixed yet.
- Functions in linked files that are still INCLUDE_ASM are listed in docs/open_questions.md (seven added
  by this batch).
- `BtlAi_GetPairRate` / `BtlAi_GetQuadRate` (btl_ai_cond.c): the switch shape that matched
  `AiThink_GetSubRate` may fix them.

## After this batch (docs/roadmap.md)

Effect tasks 0x12DD80..0x1AE200 and the stage 0x23FB20..0x248F28 complete the simulation; then
the headless PC simulation validated against the game's replay format.
