# Input

Sources: `src/sys/pad.c`, `src/sys/game_pad.c`. Layouts and bit definitions:
`include/sys/pad.h`, `include/sys/game_pad.h`. The battle input path is not linked yet; see the
note at the end.

## Pad state (verified)

`gPad[2]` at 0x333800, 0x1C0 bytes each. Two ports, slot 0, no multitap. Library: Sony libpad2
on libdbc, libvib for vibration.

| Offset | Field | Meaning |
|---|---|---|
| 0x000 | `work[0x100]` | libpad2 socket work area |
| 0x104 | `state` | 1 = usable |
| 0x108 | `phase` | 0 get vibration profile, 1 get button profile, 2 read |
| 0x114 | `data[18]` | raw report: [0],[1] buttons (active low), [2] RX, [3] RY, [4] LX, [5] LY |
| 0x130 / 0x138 | `left[2]` / `right[2]` | stick x, y as floats |
| 0x148 | `held` | |
| 0x14C | `prev` | |
| 0x150 | `pressed` | `held & ~prev` |
| 0x154 | `repeat` | |
| 0x160 / 0x164 | `vibPower` / `vibSmall` | summed per frame, sent, then cleared |
| 0x180 / 0x184 | `status` / `lastStatus` | 0 = ok, 0xFF = none |
| 0x188 / 0x18C / 0x190 | `gameHeld` / `gamePressed` / `gameRepeat` | menu input word, see below |
| 0x19C / 0x1A0 | `repeatDelay` / `repeatInterval` | init 20 / 1 |
| 0x1A4 / 0x1AC | `gameLeft[2]` / `gameRight[2]` | stick copies |

- There is no "released" word in the pad layer.
- Sticks: `raw - 127.5`, pulled toward 0 by a dead zone of 50 and clamped at 0, divided by 77.5;
  if the vector is longer than 1 it is normalised to the unit circle.
- The sticks also set digital direction bits at a 0.5 threshold on the dominant axis.
- A digital-only pad (profile F9 FF 00 00) is discarded: it reads as no input, status 0xFF.
- A disconnected port resets: buttons 0, raw sticks 0x80, floats 0. `Pad_Reset` does not clear
  `gameLeft` / `gameRight`, so those keep stale values while a pad is unplugged.
- The `Pad` struct must be declared 16-byte aligned to match.

### Raw button bits (`held` / `pressed` / `repeat`)

`((data[1] << 8) | data[0]) ^ 0xFFFF`, plus stick bits.

| Bits | Meaning | Status |
|---|---|---|
| 0x0001..0x0008 | SELECT, L3, R3, START | inferred from the standard DualShock layout |
| 0x0010..0x0080 | UP, RIGHT, DOWN, LEFT | inferred, consistent with the game word |
| 0x0100..0x0800 | L2, R2, L1, R1 | inferred |
| 0x1000..0x8000 | TRIANGLE, CIRCLE, CROSS, SQUARE | inferred |
| 0x10000..0x80000 | left stick left, right, down, up | verified |
| 0x100000..0x800000 | right stick left, right, down, up | verified |

## Menu input word (verified)

`Pad_UpdateGameButtons` builds `gameHeld` from 25 hard-coded tests on `held`. The mapping is
fixed and identical for both ports; there is no key config at this level.

| Bit(s) | Set by |
|---|---|
| 0-3 | d-pad LEFT, RIGHT, DOWN, UP |
| 4-7 | right stick LEFT, RIGHT, DOWN, UP |
| 8-11 | CIRCLE, CROSS, TRIANGLE, SQUARE |
| 12, 13 | START, SELECT |
| 14-17 | L1, L2, R1, R2 |
| 18, 19 | L3, R3 |
| 20 | tested with mask 0, never set |
| 21-24 | d-pad LEFT, RIGHT, DOWN, UP again |

The left stick's digital bits are not mapped; menus use the analog copy.

Repeat (`Pad_CalcRepeat`) works on the whole word: while `held` is non-zero and unchanged, a
timer counts down; when it goes below 0 it reloads with the interval and the whole word is
reported. Any change reloads the timer with the delay. A non-zero `pressed` always wins. Default
delay 20, interval 1. `Pad_SetRepeat` is called by the memory-card block (20, 1 and 40, 3) and
by the overlay.

There are no reader functions: about 90 overlay functions and about 20 in the main executable
read the `gPad` fields directly.

## Battle input (inferred: read from disassembly, being decompiled as `src/battle/btl_input.c`)

Fights do not use the menu word. Each fighter samples the raw pad and remaps it through a
per-player key-config table:

- A 16-byte record at fighter+0x938 tagged `'O','P','R','T'`: player, stick X, stick Y, a 32-bit
  button word and a 32-bit command word.
- Key config: `gSaveData->key[player][8]` maps eight actions to {CIRCLE, CROSS, SQUARE, TRIANGLE,
  L1, L2, R1, R2}; the default is `{2, 1, 0, 3, 4, 5, 6, 7}`.
- The record goes through an 8-entry ring per fighter; normally it is pushed and popped in the
  same call, so the delay is zero.
- When fighter+0x1278 is non-zero, input comes from fields on the fighter instead of the pad
  (AI or injected input).
- Held / previous / pressed / released words are derived per fighter, plus per-bit frame counters.

Battle-side code that reads the pad directly, bypassing the record: `func_0023F0F0` (L1 and the
sticks; looks like camera control), `func_0023F708` (sticks), `func_001D8590` (pad 0 up/down).

This section will be replaced with verified detail when `btl_input.c` is linked.
