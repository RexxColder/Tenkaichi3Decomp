# CPU opponent (AI)

Sources: `src/battle/btl_ai_mgr.c` (manager and the move action, linked),
`btl_ai_seq.c` (first AI object from 0x1B4140: step handlers 14..23, sequence runner; the
functions before 0x1B6008 are still assembly) and `btl_ai_cond.c` (start of the second object,
0x1B6D00: rule conditions and rate getters), both linked. Layout:
`include/battle/btl_ai.h` is the single definition of the AI block (`BtlAi`, per side `BtlAiWork`
with `seq`, `move`, `out`, `status`, `plan`); `btl_ai_mgr.h` adds the manager file's helper types.

Not decompiled: the rule evaluator (0x1BA760), sense and think (0x1BFF70, 0x1BAC30), most step
handlers (0x1B4C00..0x1B6008) and the virtual pad (0x1BC8A8..). Statements about those are read
from disassembly.

## Where it runs (verified)

`BtlAiMgr_Update` is called once per battle frame, after `Battle_UpdateWork` and before fighter
input is sampled. It returns at once when: paused (flag 0x100), an object load job is running
(flag 0x1000), the stage is not ready, or the sequence state is not Ready or Fight.

Per frame: measure the distance between the fighters and run a stage line-of-sight test; then
for each CPU-controlled fighter: sense, think, `BtlAi_RunSeq`, `BtlAi_SendInput`.

The AI never writes to a fighter. Its only output is `BtlCharApi_SetInjectedInput(objId,
buttons, stickX, stickY)`: the CPU plays through the same input path as a human.

## State (verified layout)

`gBtlAi`: one 0xA60 heap block. +0 AI data pointer, +4 fighter distance, +0xC sight flags,
+0x10 two per-side blocks of 0x520, +0xA50 frame counter.

Per side (`BtlAiWork`): `objId`, `type` (+4, the member's aiType), `level` (+8, its cpuLevel), the character's AI parameter pointer
(+0x18, from fighter object +0x934), then:
- `seq` (+0x28): flags, an 8-entry stack of `{id, arg}` actions, phase 0..3, step, timers;
- move work (+0xC0): keep-distance per move type, a 16-point path, rebuild timer;
- output (+0x268): buttons, stick X, stick Y, per-button toggles;
- status (+0x2C0): situation flags, opponent class and action, three 90-frame timers;
- plan (+0x2E8): rule evaluation state, eight pre-drawn rolls, a 300-frame cooldown.

## How it decides

- (read from disassembly) Decisions are data: the common AI data (in common file 2) holds an
  action table, eight rule lists and 32 per-`aiType` profiles. A rule is a 0x1C-byte record:
  a situation group, up to eight condition ids with one argument each, and an action.
- (verified) Conditions 0..34 are the `BtlAiCond_*` functions.
- (read from disassembly) Rules are re-evaluated every frame; there is no think interval.
  Slowness comes from level-scaled percentage rolls and timers.
- (verified) An action is four phase functions (init, start, run, end). The move action picks
  a target at a keep-distance from the opponent, follows a path rebuilt at most every 60
  frames, and gives up after 61 frames without progress.

## Difficulty (verified)

`BtlAi_ScaleByLevel(level, lo, hi) = lo + (int)((hi - lo) / 29.0f * max(level, 0) + 0.01f)`:
every tuning value is a pair of bytes for level 0 and level 29, linearly interpolated. Five-step
tables use `level / 6`. `aiType` selects a profile of rates, not different code. A negative
level (the training dummy) evaluates only one rule list and never reacts.

The menu's five difficulty levels map to 0, 6, 13, 21, 29 (see battle.md).

## AI button bits (read from disassembly)

1 GUARD, 2 DASH, 4 BLAST, 8 RUSH, 0x10..0x80 UP/DOWN/LEFT/RIGHT, 0x100 LOCKON, 0x200 CHARGE,
0x800 ASCEND, 0x1000 DESCEND, 0x2000 R3. The gameplay names are guesses from the default
button layout.

## Randomness and determinism

- (verified by a call scan of 0x1B4140..0x1C0400) The AI's only generator is `Rand_Range`, the
  broken-refill Mersenne Twister: 49 call sites in 40 functions.
- (verified) Conditions draw even when the result is then discarded.
- (read from disassembly) The rule evaluator draws 8 values each time it enters a new rule
  group, so the number of draws per frame depends on the situation.
- (verified by call scan) The stream is shared with about 198 call sites in the menu overlay
  and a few in the main executable.
- (verified) The AI skips any frame on which an object load job is running, so its timing
  depends on how long a load takes.
- (verified) No pad, clock or camera reads in the decompiled AI functions.
- (verified, `btl_input.c`) Replays record CPU fighters as inputs; the AI is not re-simulated.
  (inferred) During playback the AI still runs and consumes random numbers; its output is
  overwritten.

## World orientation (verified in `rigid.c`, inferred here)

+Y is down: the AI holds ASCEND when the target's Y is more than 30 below the fighter's.
