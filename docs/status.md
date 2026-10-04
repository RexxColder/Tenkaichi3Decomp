# Current status (handoff note)

Written 2026-10-04 so the work can be picked up without the conversation that produced it.
Update or delete when it goes stale.

## Verified state

- Last build verified byte-identical: the integration described in "Ninth step" at the end of this
  file (75.62% of the main executable's game code; 171 linked C files; 5877 functions diff clean;
  166 INCLUDE_ASM in linked files; DBZP 0%). Not committed by the integrator: commit after checking.
  The commit before it is the cleanup link (69.83%).
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
- `Snd_SendFighters` sends sound handles, not fighter ids (name not changed yet). The `ADXF_Tell`
  lock / worker mix-up is fixed (docs/systems/audio.md).
- Functions in linked files that are still INCLUDE_ASM are listed in docs/open_questions.md (seven added
  by the action-handler batch, 62 by the first effect / stage wave, 79 by the second, 13 by the last batch).
- `BtlAi_GetPairRate` / `BtlAi_GetQuadRate` (btl_ai_cond.c): the switch shape that matched
  `AiThink_GetSubRate` may fix them.

## After this batch (docs/roadmap.md)

Effect tasks 0x12DD80..0x1AE200 and the stage 0x23FB20..0x248F28 complete the simulation; then
the headless PC simulation validated against the game's replay format.

## Running now (written 2026-10-04, late)

Last verified and committed state: commit "Link the battle object, collision library, task tree,
text printer and AI scripts" (68.41%, 152 linked C files, 5405 functions clean, no unlinked
decompiled files, working tree clean at that commit).

Six agents were launched after that commit (cap: at most 10 at a time, user request). Their
reports arrive as messages. None may run ninja / configure.py or git; I verify and commit.

| Agent | Edits | Task |
|---|---|---|
| cleanup: battle object | src/battle/bobj_a.c, bobj_b_b.c (+ their headers) | match `BtlObjAnim_SamplePosRot`, `SamplePose`, `BtlObjXf_Update`, `BtlObjAnim_Load`, `BtlObj_BindTables`, `BObjChainB_Build`, `BObjChainB_Step`, `BObjChainA_Step` |
| cleanup: stage | stg_a.c, stg_a_b.c, eft_det_b.c, eft_det_b_b.c | `BtlStage_UpdateObjs`, `BtlStage_BreakObj`, `StgPart_Animate`, `StgFrustum_Build`, `Stg_FadeByCamDist`, `StgCol_SplitStep`, `StgCol_FighterBreakObj`, `StgCol_TraceZone`, `StgNav_FindPath`, `StgNavNode_Clear` |
| cleanup: projectiles | eft_a.c, eft_o_b.c, eft_o_c.c, eft_p.c, eft_i.c, eft_j.c, eft_r.c, eft_h.c | the `EftHit_*` near-misses, blast object, disc, sweep, `EftEmit_Spawn`, `EftBlast_Init`, `EftStruggle_Init`, shot manager |
| cleanup: fighter / AI | btl_ai_cond.c, btl_ai_act.c, btl_ai_seq.c, eft_det_b_c.c, btl_act_a.c, btl_act_c.c, btl_act_h.c, btl_capi_a.c, btl_input.c | six `AiThink_*`, `BtlAi_GetPairRate / QuadRate`, `BtlAiSense_IsBusy`, `BtlAiStep_GuardUntilSafe / Unk17`, `BtlAiSeq_PushRule`, `BtlActB_TickMemberChange`, `BtlAct_GuardHandler`, `BtlAct_GrabDash`, `BtlCharApi_HasKiBlastType2/3`, `BtlInput_Update` |
| vu0_a | NEW: config/symbols/vu0_a.txt, src/port/vu0_a.c, include/port/vu0_a.h, src/sys/vu0_a_c.c | vector / matrix library 0x11FA10..~0x121000: names, exact portable C reference (`Ref_*`), matching C for the non-VU0 functions |
| vu0_b | NEW: config/symbols/vu0_b.txt, src/port/vu0_b.c, include/port/vu0_b.h, src/sys/vu0_b_c.c | the same for ~0x121000..0x122940 |

When a cleanup agent reports: rebuild with the gate (configure, ninja exit 0, both .ok files,
both cmp silent), run fdiff over every linked file, then commit. A matched function may have
moved constants from LIT4_WORD / INCLUDE_RODATA lines into C: the ROM compare catches mistakes.
When the vu0 agents report: list vu0_a.txt / vu0_b.txt in both yamls, run
scripts/apply_names.py (all symbol files are listed, so the stock script is safe), link the
matching C files they wrote (src/sys/vu0_*_c.c) if any, rebuild with the gate, write
docs/systems/math.md additions (exact semantics, VU0 vs IEEE differences, R register rule),
commit. src/port/ is not part of the matching build.

Open question put to the user (no answer yet): after the vu0 agents report, have ONE agent
find the equivalent maths routines in the Wii build (wii/, reference only; PowerPC, no VU0)
and compare them with the PS2 reference implementations, as a cross-check of intent. Do not
launch it without a yes.

Also offered, not yet done: extracting PZS3US1.AFS and PZS3US2.AFS (2.7 GB together) from the
ISO into the gitignored disc/DATA/ when asset-format work starts.

Then continue with docs/roadmap.md, "Status 2026-10-04 (evening)", items 2..5.


## Eighth batch: the rest of the main executable (started 2026-10-04, late)

Priority changed by the user: finish the byte-for-byte decompilation before any port work (see
docs/roadmap.md). Brief: docs/briefs_remaining_main.md. Snapshot for agents:
scratchpad/snap3/asm. Both vu0 agents have reported and their files are committed but NOT
linked or listed in the yamls yet (config/symbols/vu0_a.txt, vu0_b.txt; src/sys/vu0_a_c.c,
vu0_a_c_b.c, vu0_a_c_c.c cover 0x11FA10..0x121008 with no gap; src/sys/vu0_b_c.c holds 25
non-contiguous functions of 0x121008..0x122940 and needs INCLUDE_ASM or a split for the gaps).
Link them after the four cleanup agents report (they are editing linked sources).

Launched (10 running with the four cleanup agents):

| Stem | Range | Lead |
|---|---|---|
| src/battle/hud_a | 0x2187E0..0x21CA60 | battle HUD (update 0x218D88) |
| src/battle/hud_b | 0x21CA60..0x222400 | battle HUD continued (one very large function) |
| src/sys/gfxm_a | 0x102F28..0x106D60 | low-level graphics, screen-effect group members |
| src/sys/gfxm_b | 0x106D60..0x10AD58 | depth-to-alpha, blended rectangle, texture upload helpers |
| src/battle/view_a | 0x25C2A8..0x2600B0 | code after the script commands (viewer / demo loop?) |
| src/sys/lib_a | 0x268248..0x26C050 | 516 tiny functions before the CRI library: first establish whether it is library code |

Still to launch (function-boundary cuts): 0x10AD58..0x10EC18, 0x10EC18..0x112A30,
0x112A30..0x115170 (suggested stems gfxm_c, gfxm_d, gfxm_e); 0x115478..0x1198D8,
0x1198D8..0x11EC10 (stage model / draw: stgm_a, stgm_b); 0x2129C8..0x215420 (hud_0);
0x222400..0x226488, 0x226488..0x22A750, 0x22A750..0x22FD10 (hud_c, hud_d, hud_e; the last
includes 0x22F9A8.. pause check and 0x22FC20..0x22FD10 before stg_d.c); 0x252F68..0x254A20
(the second Mersenne Twister / menu codec area: misc_a); 0x2600B0..0x263098 (view_b);
0x26C050..0x26FE90, 0x26FE90..0x273CA0 (lib_b, lib_c: only if lib_a turns out to be game code).
After the main executable: the menu overlay DBZP.BIN (0x7C204 bytes, 737 functions).

## STOP INSTRUCTION (user, 2026-10-04, late)

"when those 10 finish stop for now". Do NOT launch any new agents. For the agents still running
(three cleanup agents: stage, projectiles, fighter / AI; six new-range agents: hud_a, hud_b,
gfxm_a, gfxm_b, view_a, lib_a): when each reports, re-diff its files and record its findings in
docs/. When all cleanup agents are in: run the full gate (configure, ninja, both .ok files,
both cmp silent; a trial build while they were mid-edit did not match, which is expected),
find and fix anything that breaks the image, then commit the cleanup work (bobj_a.c and
bobj_b_b.c from the battle-object cleanup agent are edited and re-diffed but NOT committed
yet). The new-range files stay unlinked and get committed as they are. Then stop and wait for
the user. Not to be done until the user says so: linking the vu0 files, launching the queued
chunks, the menu overlay, any port work.

lib_a reported (2026-10-04): 0x268248..0x269228 is game code (confirmation dialog, 18 of 19
match, committed unlinked); 0x269228..0x273CA0 is the CRI ADX library, so the queued chunks
lib_b / lib_c are NOT needed (naming only, if ever).

## Update (2026-10-04, later): what is left before stopping

User: "you can link it after this subagent finishes then stop for now" (session budget low).
All agents have reported except the fighter / AI cleanup. Committed unlinked: vu0_a / vu0_b,
lib_a (dialog), hud_a*, hud_b, gfxm_a, gfxm_b*, view_a*. Uncommitted in the working tree: the
cleanup edits (bobj_a, bobj_b_b, eft_a / h / i / j / o_b / o_c / p / r, stg_a, stg_a_b,
eft_det_b, eft_det_b_b, stg_d.c / .h prototype fix) and the yaml .lit4 moves for eft_h, stg_a,
stg_a_b. A trial link showed text in place and ONE data shift: btl_act_h now emits 8 more
bytes of .lit4 (the fighter agent matched something), so `[0x1FDEB4, lit4, cod/1FDEB4]` must
go (btl_act_h's pool then runs 0x1FDEB0..0x1FDEBC) once that agent reports; check its report
for other pools. Then: `.venv/bin/python configure.py && ninja`, gate, fdiff sweep, commit,
stop. Linking the new-range files is the next session's first job (one integrator).

## STOPPED HERE (2026-10-04): cleanup linked, build identical, waiting for the user

All agents have finished; none are running. The four cleanup agents' work is linked and
committed: ninja exit 0, both .ok files, both images identical, fdiff sweep clean (the two
"not OK" a grep for "Error" reports are the function names `Movie_CbError` and
`File_AdxErrorCallback`). Main executable 69.83% (was 68.41%); INCLUDE_ASM in linked files
147 (was 185).

Cleanup results: battle object 7 of 8 (both pose samplers match; `BObjChainA_Step` left);
stage 10 of 10; projectiles 9 of 16 (the agent's "10 and 6" headline was miscounted: 7 left,
all checked by a differential interpreter in build/scratch_cleanup_eft/); fighter / AI 12
matched, 7 left (`AiThink_TestSkill`, `AiThink_GetBlastStep`, `BtlAiStep_GuardUntilSafe`,
`BtlAiStep_Unk17`, `BtlAiSense_IsBusy`, `BtlAct_GuardHandler`, `BtlInput_Update`). No
semantic errors found in the old attempts except the one-bit clamp constant in
`BObjChainB_Step` and the `StgRigid_Create(pos, radius, user)` argument order (fixed in
stg_d.c / stg_d.h). The agents' matching lessons are in their reports only (transcript): add
them to docs/decomp_guide.md next session; docs/open_questions.md still lists functions that
now match.

Next session, in order (ask the user before launching anything; keep to 10 agents):
1. One integrator to link the committed-but-unlinked files: vu0_a / vu0_b, lib_a (dialog),
   hud_a* + hud_b, gfxm_a + gfxm_b*, view_a*. Each agent's "for the integrator" notes are
   summarised in docs/systems/{math,graphics,hud,menu_support,audio}.md.
2. The queued chunks of the main executable (list in the "Eighth batch" section above, minus
   lib_b / lib_c which are the CRI library).
3. The menu overlay DBZP.BIN.

## Ninth batch (started 2026-10-04, user said "continue now")

Running: 10 agents (the cap).
- Integrator: linking vu0_a / vu0_b, lib_a (dialog), hud_a* + hud_b, gfxm_a + gfxm_b*, view_a*;
  also refreshing docs/open_questions.md. It edits yamls and linked sources: commit nothing of
  its work until it reports and the gate passes here.
- New ranges (new files only; snapshot scratchpad/snap3/asm; brief docs/briefs_remaining_main.md):
  gfxm_c 0x10AD58..0x10EC18 (movie player), gfxm_d 0x10EC18..0x112A30, gfxm_e
  0x112A30..0x115170, stgm_a 0x115478..0x1198D8 (stage model), mcflow_a 0x1198D8..0x11EC10
  (memory-card dialog flows), hud_0 0x2129C8..0x215420 (battle glue?), hud_c
  0x222400..0x226488, hud_d 0x226488..0x22A750, hud_e 0x22A750..0x22FD10.
Still to launch when slots free: misc_a 0x252F68..0x254A20, view_b 0x2600B0..0x263098; then
the unexplored tail before the SDK (check the yaml for what lies between 0x263098 and the
linked sys files, and 0x2BDCE8..0x2BF488 result-screen code mentioned by the view_a report),
then DBZP.BIN.

## Ninth step: the first new-range files linked (2026-10-04)

Gate passed after each module and at the end: ninja exit 0, both .ok files, both images
identical, `scripts/fdiff.py` clean on all 171 linked files (5877 functions). Main executable
75.62% (was 69.83%); INCLUDE_ASM in linked files 166 (147 + 19 that came with the new files).

- **Vector library: linked.** src/sys/vu0_a_c.c 0x11FA10, vu0_a_c_b.c 0x11FE80, vu0_a_c_c.c
  0x1204B8, vu0_b_c.c 0x121008..0x122940, no gap, no data sections. vu0_b_c.c now holds its 67
  hand-written routines as top-level assembly blocks generated from the split (INCLUDE_ASM does
  not work for VU0 code), so the file is contiguous.
- **Dialog: linked** as src/sys/dialog.c (was lib_a.c; include/sys/dialog.h, with a forwarding
  lib_a.h because src/sys/mcflow_a.c, still being written, includes the old name). Symbols split
  into config/symbols/dialog.txt and cri_adxf.txt; three CRI lock / worker names fixed.
  `.rodata` 0x2F35B0, `.sdata` 0x2FF160. `Dialog_SetCursor` stays INCLUDE_ASM.
- **HUD: linked.** hud_a.c 0x2187E0, hud_a_b.c 0x219EB0, hud_a_c.c 0x21BCA0, hud_a_d.c 0x21C0E0,
  hud_b.c 0x21CA60..0x222400. hud_a_d.c and hud_b.c are one module but were NOT merged: they use
  two views of the gauge work (`HudGauge` in hud_a.h, `HudBWork` in hud_b.c) and the image is
  the same either way; merge when the HUD headers are unified (hud_c / hud_d / hud_e are being
  written against hud_a.h). The health-field correction is applied to hud_a.h.
- **Graphics: linked.** gfxm_a.c 0x102F28, gfxm_b.c 0x106D60, gfxm_b_b.c 0x109938, gfxm_b_c.c
  0x10A6E0..0x10AD58. `.rodata` 0x2EB6A0 (gfxm_b.c), `.lit4` 0x2FC290 (gfxm_b.c) and 0x2FC2B8
  (gfxm_b_c.c), `.sdata` 0x2FE8D8 (gfxm_b_c.c, "LIT").
- **Menu support: linked.** view_a.c 0x25C2A8, view_a_b.c 0x25CFC0, view_a_c.c 0x25D290,
  view_a_d.c 0x25D468, view_a_e.c 0x25DE68..0x2600B0 (view_a_f.c merged into it: one object).

Found only by the image compare, fixed at the source:
1. gfxm_b.c, `GfxAlphaKey_BuildClut`: five of the eleven colours in the 33-byte table were wrong
   (bytes transposed). fdiff cannot see initialiser values.
2. view_a_e.c / view_a_f.c: as two objects the powers-of-ten table sat 8 bytes early; merged.
3. dialog.c: `Dialog_SetCursor` (assembly) refers to two strings of the C part by symbol; they
   are now named objects defined where the string pool has them.
4. `INCLUDE_RODATA` tables (hud_b.c, gfxm_b.c) exist only once the file has a `.rodata`
   subsegment, and a table that a subsegment boundary cuts is split: gfxm_b.c's needed its size
   (0x70) in the symbol file.
Prototype corrections in callers (all re-diffed): `TexFile_UploadOne` (btl_obj.c: uploads, returns
nothing), `StgGlare_Init` / `StgDepthTint_Init` / `GfxDepthFog_Init` / `GfxPost_DrawDepthClut`
(stg_c.c), `HudGauge_ShakeHp / ShakeKi` (hud_a.c), the GS mask callbacks (hud_b.c),
`Num_ToDigits` and `Res_RelocateOffsets` (loading.c, view_a*.c). bobj_a.c still declares
`Res_RelocateOffsets` with two arguments for one call (it matches that way).

Left as it is: asm/ still holds chunks of earlier splits (splat does not delete them); they are
assembled but not linked, and `scripts/progress.py` counts their labels, so its "functions still
in assembly" line is too high. Deleting them was not possible in the integrator's session.

