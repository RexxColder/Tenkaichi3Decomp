# Current status (handoff note)

Written 2026-10-04 so the work can be picked up without the conversation that produced it.
Update or delete when it goes stale.

## Verified state

- Last build verified byte-identical: the commit "Link the battle object, collision library, task
  tree, text printer and AI scripts" (68.41%; 152 C files; 5405 functions diff clean; DBZP 0%).
  linked, none unlinked; 5405 functions diff clean; DBZP 0%). Uncommitted by the integrator: commit after
  checking. The commit before it is "Link the second effects wave ..." (64.03%).
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
| btl_char_fx.c | 0x1CF578..0x1D1EC8 | the tail of btl_char_member.c + btl_char_fx.c + btl_char_fx_b.c: `BtlFx_UpdateGroundFx` needs `BtlChar_IsFxBitNew` defined in its file and now matches in C. `BtlFx_SpawnSpeedLines` (four LIT4_WORD) and `BtlFx_FireKiBlast` (formerly `BtlFx_SpawnDamageSparks`; last function, RODATA_ALIGN16; its constants 0x2FD200..0x2FD214 are the assembly chunk `cod/1FD200`) stay INCLUDE_ASM |
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

## Effect, stage, collision, battle-object batch: linked

Everything decompiled so far is linked: the tree has NO unlinked C file and every file in config/symbols/ is
listed in both yamls (so `scripts/apply_names.py` can be run as is). Findings are in docs/systems/; the
per-file tables of the two effect waves are in the history of this file (commits "Link the first effects wave
and the stage" and "Link the second effects wave ...").

Ranges in C (src/battle/ unless noted), with the merges made at integration:

| Range | Files | Notes |
|---|---|---|
| 0x12DD80..0x1637A0 | eft_a .. eft_m | first effect wave. Merged: the head of eft_e.c into eft_d_b.c (`EftBurst_Update`), eft_f.c into eft_e.c (`EftWaterRing_Update`), eft_l.c into eft_k.c (`EftRushShot_UpdateAttached`) |
| 0x1637A0..0x1AA7E8 | eft_n .. eft_ad_c (around btl_pool.c) | second effect wave. Merged: eft_t.c into eft_s.c, eft_u.c into eft_t_c.c, eft_v.c into eft_u_b.c, the tail of eft_y.c into eft_z.c, eft_ac.c into eft_ab_c.c. Four attempts are compiled as `ASM_STUB` definitions for their callers |
| 0x1AA7E8..0x1AE2A8 | eft_ae.c | sprite animations, the task tree, effect texture VRAM. As written; 3 INCLUDE_ASM. `.lit4` 0x2FCF04..0x2FCF14 (the chunk `cod/1FCEEC` in front holds the constants of eft_ad_b / eft_ad_c INCLUDE_ASM functions). No `.rodata`. Not merged with the head of eft_det_a.c: `EftTexSet_CheckCount` (0x1AE140, empty) is declared `__attribute__((const))` in both files and everything matches |
| 0x1AE2A8..0x1B4140 | eft_det_a, eft_det_b, eft_det_b_b, eft_det_b_c | hit detection, stage collision; eft_det_b_c.c is the head of the AI sequence object and was left a file of its own (merging it into btl_ai_seq.c changes nothing: `BtlAiSeq_PushRule` still differs by 66 of 85) |
| 0x1B4140..0x1B6D50 | btl_ai_seq.c | btl_ai_seq_a.c merged in, replacing the 38 INCLUDE_ASM lines; the table `D_002ED970` is now a local initialiser. 2 INCLUDE_ASM (`BtlAiStep_GuardUntilSafe`, `BtlAiStep_Unk17`). `.rodata` 0x2ED8C0 (0x1B0 bytes), `.lit4` 0x2FCFDC..0x2FCFF8 (the former chunk `cod/1FCFDC` was this file's constants, not eft_det_b's) |
| 0x22FD10..0x236190, 0x115170..0x115478 | stg_d.c, col_a.c, stg_d_b.c | rigid bodies, collision primitives part one |
| 0x236190..0x239BB0 | col_b.c | collision primitives part two, as written, no INCLUDE_ASM. `.lit4` 0x2FE48C..0x2FE4B8, `.sdata` 0x2FEBAC (4 bytes, the compiler-pooled 0x7F7FFFFE) |
| 0x239BB0..0x23C310 | col_c.c | the text printer: col_b_b.c (packet helpers) merged into the top, `Font_Flush` now matches in C. No INCLUDE_ASM. `.rodata` 0x2F21A0..0x2F2220 (four jump tables), `.lit4` 0x2FE4B8 (one constant); 0x2FE4BC..0x2FE4CC is `DemoCam_Update`'s (chunk `cod/1FE4BC`) |
| 0x23C310..0x23D1E8 | col_c_b.c | inline icon tags; 1 INCLUDE_ASM (`FontIcon_PutSprite`), no data |
| 0x23FB20..0x248F28 | stg_a, stg_a_b, stg_b, stg_c (and the assembly chunk `cod/140C68`, VU0 macro code) | stage |
| 0x24BBE8..0x250B28 | bobj_a.c | bobj_a.c + bobj_b.c: `BtlObj_UpdateFace` and `BtlObj_IsJawActive` now match in C (they need `BtlObjFace_StepBlink` 0x24EB70 / `BtlObjMdl_HasJaw` 0x24E9C8 defined above them; verified with the real bodies). 5 INCLUDE_ASM. `.rodata` 0x2F2420..0x2F2AA0 (the table `D_002F2420` by INCLUDE_RODATA, then the jump tables), `.lit4` 0x2FE640..0x2FE67C (two LIT4_WORD) |
| 0x250B28..0x2527B0 | bobj_b_b.c | secondary motion chains, as written; 3 INCLUDE_ASM. `.lit4` 0x2FE67C..0x2FE718 (28 LIT4_WORD); `BObjChainA_Step`'s 19 constants 0x2FE718..0x2FE764 are the chunk `cod/1FE718`. The tables 0x2F2AA0..0x2F2EC0 stay the chunk `cod/1F2AA0` |

Functions still INCLUDE_ASM in linked files: 185 (183 with a C attempt next to them), listed in
docs/open_questions.md.

Names: `eft_ae.txt`, `col_b.txt`, `col_c.txt`, `bobj_a.txt`, `bobj_b.txt`, `btl_ai_seq_a.txt` were listed last; no
duplicate names or addresses in any symbol file. The name pass replaced 1233 placeholder uses in 111 linked
files (`BtlTask_*`, `EftVram_*`, `BtlObjAnim_*`, `BtlObjXf_*`, `BtlObj_GetNode`, `Font_*`, `ColCapsule_Set`,
`ColSphere_Set`, the AI sequence handlers ..). No wrong callee name turned up at link.

Prelude: `__gp_forget` in include/gcc_prelude.inc uses `.set mips64` since the first effect wave (see
decomp_guide.md).

Evidence of object boundaries not acted on: eft_v_b.c .. eft_y.c (0x1871A8..0x195038) may be one object;
0x1ADBA8..0x1AE5F8 (the tail of eft_ae.c and the head of eft_det_a.c) is one source file; eft_det_b_c.c +
btl_ai_seq.c are one object starting at 0x1B3F78; stg_d may extend back to 0x22FC40; col_a probably starts at
0x230B10; eft_ae.c holds three objects (cuts at 0x1AA818, 0x1AD138, 0x1ADBA8) and its first function belongs to
eft_ad_c.c's object.

Not decompiled and not assigned: 0x22FC40..0x22FD10 (two stage rigid list helpers plus one function of another
module).

## Known follow-ups

- docs/systems/ still uses the old names of the function and task flags renamed at the second effects
  integration (`BtlFx_SpawnDamageSparks`, `EFT_TASK_HIT_2` ..).
- eft_y.c / the head of eft_z.c (`EftLine_*`) and the rest of eft_z.c (`EftBill_*`) are one module (part
  kind 16) under two prefixes: unify.
- `EftShotFx_IsOnScreen` (eft_t_c.c): its `#if 0` attempt does not compile when enabled (parse error).
- `EftBlast_Init` (eft_j.c) matches when compiled with `-fno-gcse` (agent's note): look for a source
  form that defeats gcse there.
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
- The previous action (fighter +0x950) has no writer anywhere (searched the whole executable);
  see combat.md. Fix the comment-level claims in handlers that assume it works.
- One unified fighter header: every battle file has its own partial view of the 0x1600-byte
  fighter object; the merged picture is in docs/systems/fighter.md and combat.md.
- Names proposed by the script-command agent for `BtlFacade_*` placeholders (lip sync, ki and
  blast gauge adders, CPU level) are not applied yet.
- `Snd_SendFighters` sends sound handles, not fighter ids; `ADXF_Tell` in
  config/symbol_addrs.txt may be the inner unlocked function. Neither is fixed yet.
- Functions in linked files that are still INCLUDE_ASM are listed in docs/open_questions.md (seven added
  by the action-handler batch, 62 by the first effect / stage wave, 79 by the second, 13 by the last batch).
- `BtlAi_GetPairRate` / `BtlAi_GetQuadRate` (btl_ai_cond.c): the switch shape that matched
  `AiThink_GetSubRate` may fix them.

## After this batch (docs/roadmap.md)

Effect tasks 0x12DD80..0x1AE200 and the stage 0x23FB20..0x248F28 complete the simulation; then
the headless PC simulation validated against the game's replay format.
