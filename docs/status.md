# Current status (handoff note)

Written 2026-10-04 so the work can be picked up without the conversation that produced it.
Update or delete when it goes stale.

## Verified state

- Last build verified byte-identical: the commit "Link the first effects wave and the stage"
  (47.79% of the main executable's game code in C; 110 C files linked; 3855 functions diff clean).
  uncommitted by the integrator: commit after checking.
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

First wave: LINKED, build byte-identical (0x12DD80..0x1637A0 and 0x23FB20..0x248F28, all of it).
Findings are recorded in docs/systems/effects_stage.md. Files as linked (src/battle/), with what
changed at integration:

| File | Range | Notes |
|---|---|---|
| eft_a.c | 0x12DD80..0x132290 | as written; 10 INCLUDE_ASM. `.rodata` 0x2EC4E0, `.lit4` 0x2FC340..0x2FC364; the constants at 0x2FC364..0x2FC398 (its last one and eft_b's first twelve, all owned by INCLUDE_ASM functions) are the assembly chunk `cod/1FC364` |
| eft_b.c | 0x132290..0x136760 | as written; 6 INCLUDE_ASM. `.rodata` 0x2EC620, `.lit4` 0x2FC398..0x2FC3FC |
| eft_c.c | 0x136760..0x13A9D0 | as written; 2 INCLUDE_ASM (their constants are in the chunk `cod/1FC3FC`). It called `EftSurf_DrawPolyOtClipped` by a name that did not exist (fixed) |
| eft_d.c | 0x13A9D0..0x13C300 | as written; 3 INCLUDE_ASM (LIT4_WORD each). Not merged into eft_c.c |
| eft_d_b.c | 0x13C300..0x13F430 | eft_d_b.c + the head of eft_e.c (the transition task): `EftBurst_Update` now matches in C (needs `EftBurst_InitFlash/InitRing/InitDebris` in its file AND the prelude change below). 2 INCLUDE_ASM. Emits `.sdata` 0x2FE9D0..0x2FE9DC |
| eft_e.c | 0x13F430..0x147050 | rest of eft_e.c (steam, water) + eft_f.c: `EftWaterRing_Update` now matches in C (needs `EftWater_GetSurfaceY`). 5 INCLUDE_ASM; the last one's constants are in the chunk `cod/1FC708` |
| eft_g.c | 0x147050..0x14B108 | as written; 8 INCLUDE_ASM; emits one constant (0x2FC714), the rest of its pool is assembly (`cod/1FC708`, `cod/1FC718`) |
| eft_h.c | 0x14B108..0x14F230 | as written; 7 INCLUDE_ASM |
| eft_i.c | 0x14F230..0x1532A0 | as written; 7 INCLUDE_ASM |
| eft_j.c | 0x1532A0..0x157398 | as written; `EftBlast_Init` INCLUDE_ASM |
| eft_k.c | 0x157398..0x15C728 | eft_k.c + eft_l.c: `EftRushShot_UpdateAttached` matches only in this unit. The two tables at 0x2ECB60..0x2ECBA0 are referenced as externs and stay the assembly chunk `cod/1ECB60` |
| eft_l_b.c, eft_l_c.c, eft_l_d.c, eft_m.c | 0x15C728.., 0x15E5D0.., 0x15EF18.., 0x15F728..0x1637A0 | as written, no INCLUDE_ASM. eft_l_d.c and eft_m.c not merged (nothing needs it) |
| stg_a.c | 0x23FB20..0x240C68 | first part of the agent's stg_a.c; 2 INCLUDE_ASM (constants in the chunk `cod/1FE588`) |
| (assembly `cod/140C68`) | 0x240C68..0x240DB8 | `StgVu_RotateZ/X/Y`: VU0 macro code that cannot be INCLUDE_ASM (splat writes the accumulator operand as `ACC` in per-function files, which the assembler rejects) |
| stg_a_b.c | 0x240DB8..0x242D28 | rest of stg_a.c with a copy of its preamble; 3 INCLUDE_ASM |
| stg_b.c, stg_c.c | 0x242D28.., 0x245F58..0x248F28 | as written; 4 and 2 INCLUDE_ASM |

62 functions of this wave stay INCLUDE_ASM (docs/open_questions.md).

Prelude: `__gp_forget` in include/gcc_prelude.inc now uses `.set mips64` instead of `.set mips4`
(removes the hazard nop the assembler left after `mfhi` in front of `mtc1`). Tested before adoption by
assembling the compiler output of all 117 linked objects with both preludes: only the object holding
`EftBurst_Update` differs.

Names: every symbol file of the wave (`eft_a.txt` .. `eft_m.txt`, `stg_a.txt` .. `stg_c.txt`) is listed
in both yamls; there were no duplicate names or addresses among them. The placeholders in linked files
(`func_0014AB90`, the `EftBurst_*`, `EftWater_*`, `EftHit_*`, `EftCam_*`, `EftShot_*` callees of
battle_load.c, btl_scene.c, btl_char_fx.c, btl_char_member.c) were replaced by the name pass.
stg_a.c used `StgRigid_*` names that exist only in the unlisted `stg_d.txt`; they are back to
`func_002302F0 / 328 / 398 / 3C8 / 4C0 / 908` until stg_d is linked.

Merges tried and not made: eft_i + eft_j (makes `EftBlast_PostUpdate` differ by one instruction, so
eft_j is NOT in eft_i's file), eft_j + eft_k and eft_a + eft_b (no function gains). eft_c + eft_d,
eft_g + eft_h + eft_i, eft_b + eft_c, eft_l_d + eft_m and the stage files were not tried: their
INCLUDE_ASM functions differ by register allocation or store order, not by the branch-likely /
delay-slot symptom a merge fixes, and each of these merges needs hand-written cast macros.

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

ALL nineteen relaunched effect / detection / stage agents and eft_ad have reported; every file
was re-diffed and is documented in docs/systems/effects_stage.md (not linked): stg_d (2 files),
eft_det_a, eft_det_b (3), eft_n, eft_o (3), eft_p (2), eft_q, eft_r, eft_s, eft_t (3),
eft_u (2), eft_v (3), eft_w, eft_x (3), eft_y, eft_z (3), eft_aa, eft_ab (3), eft_ac, eft_ad (3),
col_a.

Still running: eft_ae (0x1AA7E8..0x1AE2A8, corrected end), col_b, col_c, bobj_a, bobj_b,
btl_ai_seq_a (0x1B4140..0x1B6008, new file to merge into btl_ai_seq.c).
The user asked (2026-10-04) for at most 10 subagents at a time.

Merge evidence for the next integration: eft_m + eft_n (+ eft_o.c: aura, lightning);
eft_o_c + eft_p.c (disc); eft_p_b + eft_q head (glow); eft_s (chain) + eft_t.c head
(`EftChain_SetRes` needs 0x1793A8); eft_t_c + eft_u.c (teleport lines; eft_u needs eft_t's
tables non-const); eft_u_b + eft_v.c (particle emitter; `EftPtcl_SetTexture`); eft_v_c +
eft_w head (sprite chain; `EftLink_SetTex`); eft_w tail + eft_x.c (part kind 10); eft_x_c +
eft_y head (quads); eft_y tail + eft_z.c head (`EftBill_SetTexture` needs 0x195038; unify the
EftLine / EftBill names); eft_ab_c + eft_ac (ribbon: four functions); eft_ac tail + eft_ad.c
(zap); eft_ae + eft_det_a head (`func_001AE140`); stg_d may extend back to 0x22FC40; col_a
probably starts at 0x230B10. Renames to apply: `BtlFx_SpawnDamageSparks` -> ki blast launcher
name; the eft_a.h task flag names (see effects_stage.md "Projectile hit detection").
Also check files with repeated identical constant initialisers for merged rodata (eft_e,
eft_l_d, eft_z, eft_aa, eft_u): compare each object's .rodata size with the original.

NOT launched yet: 0x22FC40..0x22FD10 (two stage rigid list helpers plus one function of
another module).

## Known follow-ups

- stg_a.c / stg_a_b.c call the stage rigid bodies by placeholder (`func_002302F0` ..); the name pass
  restores `StgRigid_*` once stg_d.txt is listed.
- `EftBlast_Init` (eft_j.c) matches when compiled with `-fno-gcse` (agent's note): look for a source
  form that defeats gcse there.
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
  by the action-handler batch, 62 by the first effect / stage wave).
- `BtlAi_GetPairRate` / `BtlAi_GetQuadRate` (btl_ai_cond.c): the switch shape that matched
  `AiThink_GetSubRate` may fix them.

## After this batch (docs/roadmap.md)

Effect tasks 0x12DD80..0x1AE200 and the stage 0x23FB20..0x248F28 complete the simulation; then
the headless PC simulation validated against the game's replay format.
