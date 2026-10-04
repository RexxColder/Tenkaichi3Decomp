# Menu overlay (DBZP.BIN)

Loaded at 0x334C00; code 0x334C00..0x3B0E04 (737 functions), data from 0x3B0E80. Built with
-G0 (verified: no gp-relative access anywhere). Data is laid out per object as `.data` then
`.rodata`. Source goes in src/menu/. Brief and chunk table: docs/briefs_menu_overlay.md.

## Entry and mode dispatcher (src/menu/menu_a_b.c; verified by matching C)

`Game_Main` calls `Progress_Main` (0x336A90) after `Overlay_Load(0)`; when it returns,
`Game_Main` runs `Battle_Main(0)`. `Progress_Main` loops `switch (gProgress->mode)` through
the 70-entry table at 0x3B1130 until a handler returns non-zero.

| Mode | Handler | Ends the loop |
|---|---|---|
| 1 | `Title_Run(1)` (0x337D70) | on result 1 (attract demo set up) |
| 2 | opening movie, then mode 1 | no |
| 4 | `MainMenu_Run(2)` (0x336838) | no |
| 6..10 | 0x33CBF8 | if non-zero |
| 13..30 | 0x379A58 | if non-zero |
| 33..35 | 0x362160 | if non-zero |
| 38..41 | 0x352CB8 | if non-zero |
| 44..45 | 0x3591C8 | if non-zero |
| 48..50 | 0x39E940 | never |
| 53..56 | 0x3A9850 | if non-zero |
| 60 | 0x3590A8 | never |
| 62 | 0x39FAA8 | never |
| 70 | `Shen_Main` (main executable 0x2BD230) | never |
| others | nothing: the loop spins forever |

Main-menu item -> mode: 0 -> 6, 1 -> 13, 2 -> 33, 3 -> 38, 5 -> 48, 6 -> 44, 7 -> 53, 8 -> 60,
9 -> 62, 10 -> 70 (item 10 only with all seven dragon balls, `gSaveData->unlockFlags & 0x7F`).
(inferred) which game mode each is.

Before the loop: `Pad_SetRepeat(0x28, 3)`; first-run partition load (0x35DF70) when
`gProgress->flags & 0x40`; the return screen after a battle is chosen from
`BattleResult_GetReason()` when `BattleResult_GetFlags() & 8` (bit 8 -> mode 4; 0x10 -> 39 /
40 / 45 by current mode; 0x80 -> 38; 0x800 -> 6; 0x1000 -> 56); sound bank file 0x14C into
bank 2. Twelve archive pointers at 0x3B0E84..0x3B0EB0, one per group of modes, freed on exit.

`gProgress` fields (verified): +0x04 base file id; +0x14 flags (0x40 first run, 0x100 freeze,
8 title left with result 0); +0x18 mode; +0x28 attract-demo counter; +0x2C last main-menu
item; +0x34 sub menu; +0x38 last sub-menu item.

## Screens of chunk 1 (0x334C00..0x339610; all 25 functions match)

- **Title** (mode 1, menu_a_c.c, `gTitle` 0xD0 bytes): still picture 300 frames, then the
  movie (intro, "press start", menu of 1 or 2 items). Item 0 runs `McFlow_Start(3)`. After
  1800 idle frames: `Demo_SetupBattle()` and leave (attract demo), twice, then the opening
  movie replays. That call is the only battle hand-off in this chunk.
- **MainMenu** (mode 4, menu_a.c, `gMainMenu` 0x188 bytes): a ring of items with six plates
  shown, four guide characters, the seven dragon balls from `unlockFlags`.
- **ModeMenu** (menu_a_d.c, head only; continues in chunk 2; `gModeMenu` 0x33C bytes): the
  sub menu chosen by `gProgress->subMenu` (0..7), up to 16 items, a picture per item loaded
  in the background; `gSaveData->slot[n].val[0..2]` = listed / cleared / new bit sets.
- Common: pad 0 only (`gameRepeat` 8 up, 4 down; `gamePressed` 0x200 confirm, 0x400 cancel,
  0x1000 start); `gProgress->flags & 0x100` freezes update and input; Title and MainMenu call
  `Gfx_EndFrame(1)`. Random draws all from `Rand_Range` (the shared Mersenne Twister): title
  voice choice, guide choice at every cursor move, blink timers, `FlashAnim_Blink / Talk`.
  No clock reads.
- Menu asset layout: a pack is a table of byte offsets; a screen's section is compressed
  (`Sprite_Unpack`) into an inner pack of texture lists, one movie, text and subtitles.
- Original oddity: an unhandled mode value hangs `Progress_Main` in a busy loop.
