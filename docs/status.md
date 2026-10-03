# Current status (handoff note)

Written 2026-10-04 so the work can be picked up without the conversation that produced it.
Update or delete when it goes stale.

## Verified state

- Last commit that was built and verified byte-identical: `efe4147` (12.49% of the main
  executable's game code in C; 61 C files linked; 1509 functions diff clean).
- Later commits only touch docs and tooling.
- Check at any time: `.venv/bin/python configure.py && ninja`, then
  `cmp build/SLUS_216.78.rom disc/SLUS_216.78.rom` and `cmp build/DBZP.BIN disc/BIN/DBZP.BIN`,
  then `python3 scripts/progress.py`.

## In the tree but not linked or committed (fighter-core batch)

All match per function according to their agents; files without INCLUDE_ASM were re-diffed and
confirmed. An integration agent is linking them (brief: link each, merge the pairs below,
extend the prelude, rename the guard functions).

| Files (src/battle/) | Range | Notes |
|---|---|---|
| btl_script_cmd.c | 0x259EC8..0x25C2A8 | 46/46 |
| btl_char_status.c, btl_char_status_anim.c | 0x1C2FF0..0x1C4BF8 | 79/79 |
| btl_char_hit.c + btl_char_coll.c | 0x1C7B30..0x1CA6D0 | merge: coll.c's four functions belong to hit.c's object |
| btl_char_coll_b.c + btl_char_member.c | 0x1CA6D0..0x1D00D8 | `BtlMember_Damage` needs `BtlColl_NextPoolMember` in the same file |
| btl_char_fx.c, _b.c, _c.c | 0x1D00D8..0x1D3B40 | 64/67; reported after the integrator started, so NOT in its brief: integrate separately. `BtlFx_UpdateGroundFx` needs the fx-bit helpers of btl_char_member.c in the same file |
| btl_input.c (linked) + btl_char_ctl.c | 0x1D3B40..0x1D60A0 | merge: `BtlInput_TestAction` needs btl_input.c's readers |
| btl_char_ctl_b.c, _c.c, _d.c | 0x1D60A0..0x1D8330 | all match |
| btl_char_flag_clash.c, _snd.c, btl_char_flag.c, _opp.c | 0x1D8750..0x1DBF20 | one function blocked by a prelude gap |
| btl_char_move.c | 0x1DC9A0..0x1E0290 | 47/47 |
| btl_char_action.c | 0x1E0290..0x1E3158 | 49/49 |
| btl_ai_act.c | 0x1BC8A8..0x1C0058 | 55/56 |
| btl_ai_think.c | 0x1B80F8..0x1BB128 | 36/43; one blocked by the prelude gap |

Prelude gap still open: a `symbol(reg)` load directly before an unfilled `jr $ra` or `b` is not
moved into the delay slot (affects `BtlCharSnd_GetBankMask`, `AiThink_FindWeightColumn`).

Rename pending: `BtlColl_GetChangeKind` / `BtlColl_IsChanging` are guard-kind tests
(`BtlColl_GetGuardKind` / `BtlColl_IsGuarding`).

## Running when this was written

- One integration agent (above).
- Fourteen decomp agents on the rest of the fighter code, brief in
  `docs/briefs_action_handlers.md`; each writes `src/battle/<stem>.c`, a header and
  `config/symbols/<stem>.txt`:

| Stem | Range |
|---|---|
| btl_act_a .. btl_act_j | 0x1E3158..0x204E78 in ten chunks (cuts at 0x1E6CC0, 0x1EA5F8, 0x1EE058, 0x1F1930, 0x1F5460, 0x1F8C00, 0x1FC2B0, 0x1FFAC0, 0x203168): the 115 action handlers and the input decision code |
| btl_capi_a | 0x204E78..0x207020 (by-object-id fighter API, first part) |
| btl_capi_b | 0x208430..0x20BA80 (API continued, script control) |
| btl_tech_a | 0x20BA80..0x20F0E8 (attack record accessors, gauge rates) |
| btl_tech_b | 0x20F0E8..0x2129C8 (technique slot readers, HUD wrappers) |

Their reports arrive as messages. For each: re-run `scripts/fdiff.py` on its files, record the
findings in `docs/systems/`, then integrate (or hand the batch to an integration agent).

## Known follow-ups

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
