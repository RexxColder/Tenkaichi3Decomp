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
| btl_act_j.c | 0x203168..0x204E78 | 28/28; 9 jump tables 0x2F11D0..0x2F15F8, .lit4 0x2FE070..0x2FE078 |

`scripts/apply_names.py` reads every file in config/symbols/, including those of agents whose
files are not in the yamls yet: run it only after listing them, or with their stems as skip
arguments.

## Running when this was written

- Fourteen decomp agents (btl_act_j has reported) on the rest of the fighter code, brief in
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
