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
Reported and documented: eft_a, eft_b, eft_c, eft_e, eft_f, eft_g, eft_i, eft_j, eft_k, eft_m, stg_a, stg_b, stg_c. Still running:
eft_d, eft_h, eft_l.

Second wave: launched, then STOPPED after a few minutes (2026-10-04) to stay inside the usage
limit. Relaunch these from scratch after the reset; any partial `eft_n`..`eft_u`, `eft_det_*`
or `stg_d*` files on disk are unverified leftovers and can be overwritten. The prompts are the
shared brief plus these ranges and leads:

| Stem | Range |
|---|---|
| eft_det_a | 0x1AE200..0x1B16F0 (hit detection of hit records against fighters, 0x1AFDB0..0x1B10F0; `func_001AF9C0` is called by the battle loop between scene update and post-update) |
| eft_det_b | 0x1B16F0..0x1B4140 (stage tests: `func_001B2DF0` segment against stage, 0x1B24B8 fighter stage-break tests; may run into AI code) |
| stg_d | 0x22FD10..0x230AA0 (stage rigid bodies / debris, 0x2309A8 is called by `BtlStage_Update`) plus func_00115370 in stg_d_b.c |
| eft_n | 0x1637A0..0x167E68 (aura, second half; first half is eft_m) |
| eft_o | 0x167E68..0x16C2E0 (part kind 2; 0x1699D0 creates hit records: blast objects of `EftShotTech`) |
| eft_p | 0x16C2E0..0x170A50 (0x16C788 creates hit records: the pieces of `EftMulti`, 0x16D858..) |
| eft_q | 0x170A50..0x174A70 (aura constructor 0x170E00, aimed effect 0x171C78, flash 0x172480) |
| eft_r | 0x174A70..0x178AB0 (KI BLASTS: hit record creators 0x1763E8, 0x177548, 0x178530; deflect at 0x1764E8 / 0x178630) |
| eft_s | 0x178AB0..0x17CB40 |
| eft_t | 0x17CB40..0x180BF8 (part kinds 18 and 0; 0x17D500 request 0x18; 0x180A00 requests 0xC..0xF) |
| eft_u | 0x180BF8..0x1853C8 |

NOT launched yet: eft_v 0x1853C8..0x1895E8 (holds the impact effect
0x187BE0, task class 0x2C3F20: establish whether it is visual), then nine more chunks
0x1895E8..0x1AE200 with cuts at 0x18D618, 0x191D28, 0x195EE8, 0x199F28, 0x19E0C0, 0x1A21A8,
0x1A62C8, 0x1AA7E8 (suggested stems eft_w, eft_x, eft_y, eft_z, eft_aa .. eft_ae).

Merge evidence so far: eft_e + eft_f are one source file; eft_h + eft_i continue each other.

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
