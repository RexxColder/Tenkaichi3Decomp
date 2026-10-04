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

## Story mode ("Hist"), modes 6..10 and 3 (chunk 2, 0x339610..0x33E108; src/menu/menu_b*.c; all 22 functions match)

Names are guesses from behaviour. Files: menu_b.c = ModeMenu tail (must be appended to
menu_a_d.c: one source file, shared string; merged test in build/scratch_menu_b/), menu_b_b.c
= `ModeBg` (scrolling backdrop), menu_b_c.c = `HistOutro` + `Hist_Main`, menu_b_d.c = head of
`HistSel` (continues in chunk 3).

`Hist_Main` (0x33CBF8; verified) loads archive 2 (file baseFile + 0xC) and runs:

| Mode | Screen | Next |
|---|---|---|
| 6 | saga select (`HistSel`, 0x33F440) | 0 -> mode 4; 1 -> 7; 2 -> 3 |
| 7 | episode list (`ModeMenu_Run`) | non-zero -> mode 8 and RETURN 1 (battle); 0 -> 6 |
| 8 | result (`HistResult_Run`) | 0 -> 7; 1 -> 10; 2 -> 9 |
| 9 | saga outro (`HistOutro_Run`) | 0 -> 10; 1 -> 3 |
| 10 | save (`HistSave_Run`) | 0 -> 6; 1 -> 7 |
| 3 | ending movie | back to 6 or 10 |

`gProgress->subMenu` is the saga (0..7; episodes per saga 3, 4, 7, 5, 16, 5, 4, 4 = 48).

**Battle hand-off (verified)**: `BattleSetup_Clear()` then
`BattleSetup_SetScript(ModeMenu_GetLine(saga, episode))` (`setup.script` = the episode's
running index + 1). Nothing else is written to the battle setup: a story battle is defined
entirely by its script number. The episode's "new" bit (`slot[saga].val[2]`) is cleared.

Other verified facts: saga pack = file baseFile + 0x10 + saga (sections: 1 picture, 2/3
textures, 4 message text, 5 subtitles, 6 description table of 0x20-byte entries, 7/8 guide
faces, 9 movie, 10 backdrop textures); episode pictures = file imageBase + episode.
`SaveSlot.flags`: 1 saga unlocked, 4 outro seen, 0x10 outro script started, 0x20 excluded
from the random event, 0x40 first-visit greeting; `slot[n].val[1]` cleared bits give the
completion percentage. Saga select random event: `Rand_Range(100) < 4` per eligible saga per
visit when `unlockFlags & 0x80`. All random draws are `Rand_Range`; pad 0 only.

Correction to the note above: the overlay's `.rodata` is per source file, but its `.data`
(the work pointers) is grouped per group of modules (0x3B12F0..0x3B1310 holds the six story
work pointers; 0x3B0E80 those of MainMenu / Title and the archive pointers).

## Story mode continued (chunk 3, 0x33E108..0x342588; src/menu/menu_c*.c; all 29 functions match)

Files: menu_c.c = tail of `HistSel` (same object as menu_b_d.c: merge them and delete the
stub copy of `HistSel_PlayVoice`), menu_c_b.c = `HistGuide` (the saga-select guide scripts: a
switch with 363 case labels), menu_c_c.c = `HistResult` (mode 8), menu_c_d.c = `HistSave`
(mode 10), menu_c_e.c = head of the `CharSel` object (continues in chunk 4). menu_c.h's
`HistSel` (full layout) replaces the head-only view in menu_b.h.

Verified by matching C:
- Saga select (`HistSel`, mode 6): Goku guides; a two-item menu (saga list / level); leaving
  with a saga writes `gProgress->subMenu` and `gProgress + 0x3C` = the save's level (inferred:
  difficulty). Nothing here touches the battle setup.
- Result screen (`HistResult`, mode 8): win = `BattleResult_GetFlags() & 1`; a win sets the
  episode's cleared bit. Points = `gProgress->reward.points[level]`, doubled while item 0x88
  is owned, paid with `Save_AddMoney` 33 a frame. `gProgress + 0x40` reward block:
  `points[3]`, `item[3]` (+0x4C), `chara[3]` (+0x58), `stage[3]` (+0x64), `episode[3]` (+0x70),
  -1 = none (filled by the battle script). Rewards go into the save at once (`Save_AddItem`,
  `charaBits`, `stageBits`, new saga / episode bits, dragon ball from `BattleResult.unk44`).
- Save screen (`HistSave`, mode 10): `McFlow_Start(0)` on a black screen.
- `SaveSlot.flags`: 1 listed, 2 new, 4 outro seen, 8 guide introduced, 0x20 event done,
  0x40 new episode; `unlockFlags` 0x80 first-visit explanation seen, 0x100 100 % speech,
  0x200 new saga.
- **Save layout**: `SaveData` is `{ s32 sum[2]; struct { s32 unlockFlags; s32 level;
  SaveSlot slot[9]; ... } body; }`: the nested body reproduces the original address
  arithmetic (`MSave` / `MSAVE` in include/menu/menu_c.h; should move to include/sys/save.h).
- Random draws: `Rand_Range` only (greeting line, ambient timer, result picture
  0x3FE + `Rand_Range(3)`).
- (inferred) The mode is "Dragon History"; `GetWin_IsAnimating` may really mean "animation
  finished".

## Character select hand-off and team select (chunk 5, 0x348710..0x34D368; src/menu/menu_e.c, menu_e_b.c; all 19 functions match)

One object runs 0x342190..0x351C38: `CharSel` (chunks 3 tail, 4, and `CharSel_Run` here), then
`TeamSel` (head here, rest in chunk 6). Names are guesses. Layouts in include/menu/menu_e.h
(`TeamSel` 0x3EA4 bytes; menu_f.h has a second view to unify).

**Versus battle hand-off (`CharSel_Run`, 0x348710; verified by matching C).** Called by the
handlers of modes 38..41 and 44..45. On leaving, the choices go to `gProgress` (+0x440 /
+0x530 the two sides' 0x30-byte records, +0x628 stage cursor, +0x62C music id), then:
1. Stage 0x23 = random: `stageIds[Rand_Range(stageCount - 1)]` until below 0x23. Music 0x18 =
   random: `Rand_Range(9) + 8`. (Shared Mersenne Twister, drawn before the battle starts.)
2. `gProgress->mode == 45`: battle mode 6, no time limit, announcer 1, cpuLevel -1. Otherwise
   battle mode 0, screenMode = (players == 1), and from `gSaveData->rule[0..5]`: time limit
   (0 -> 1, 1 -> 2, 2 -> 3, 3 -> 4, 4 -> 0), `CpuLevel_FromSetting(rule[1])`, announcer,
   two side flags, one more flag.
3. `Battle_ClearWork()`, `BattleSetup_SetRule(screenMode, mode, bgm, timeLimit, announcer,
   stage, unk10)`.
4. `BattleSetup_SetSide` by `players`: 0 = pad vs CPU, 1 = pad vs pad, 2 = CPU vs CPU
   (control 0 = pad, 2 = CPU).
5. Per side: `BattleSetup_SetMember(side, 0, rec->chara, rec->color, 0, cpuLevel, 100.0f,
   rec->items)`; then `BattleSetup_Finish()`.
This is the complete set of inputs a versus fight starts from (with the random stage / music
resolved in the menu): what netplay peers must agree on before the simulation starts.

Team select (`TeamSel`, verified unless marked): screen pack = a section of `gMenuArc5` with
57 sections (textures, movies, stage id list, master character grid, 165 chip texture lists,
item panel data, music id list); portraits = file 0x2F9 + character, stage pictures = file
0x39D + stage. Restores both teams (five 0x30-byte records each), players (+0x620), battle
type (+0x624), stage, music and DP level (+0x630) from `gProgress`. DP battles: limit 10 /
15 / 20 by level, `TeamSel_FitsDp` = cost + `ChrTbl_GetCost(chara)` <= limit;
`TeamSel_IsCharaFree` refuses a character `ChrTbl_IsRelated` to another member. Member record
(0x30 bytes): +0x18 costume, +0x1C character id (-1 none), +0x20 `BattleItemSet`.

## Character / stage / music select (chunk 4, 0x342588..0x348710; src/menu/menu_d.c; 11 of 12 match)

`CharSel_Input` (0x3456A8, 0x3064 bytes) is INCLUDE_ASM: its attempt differs in 3 of 3097
instructions (a `lui` in a delay slot), so what is said about input below is from that
attempt. `gCharSel` = 0x3B38D4, 0x3A4C bytes; layouts in include/menu/menu_d.h. The file only
lays out correctly merged behind menu_c_e.c (one object 0x342190..0x348D78).

- (verified) Per-side pick record kept in `gProgress` (+0x440 / +0x530, first 0x30 bytes of a
  0xF0-byte side record): col, row, form, customCol, customRow, item set, costume, picked
  character id, items[8]. `gProgress + 0x620` pad mode (1 = one pad per side; 0 or 2 = pad 0
  picks both sides in turn).
- (verified) Grid markers as this screen uses them: 0xA1 random, 0xA2 locked, 0xA3 saved
  custom characters, 0xA4 filler (**view_a.h has 0xA1 / 0xA3 swapped**). Music list id 0x18 =
  random, 0x19 = locked (`bgmBits` confirmed as the music unlock bits).
- (from the attempt) Side states: 0 grid, 1 form reel, 2 item set, 3 item panel, 4 item info,
  5 costume, 6 saved custom characters, 7 done; then stage (8) and music (9) on pad 0.
  Confirming the costume of the random cell draws `Rand_Range(masterCount)` until a real
  character, then `Rand_Range(formCount)`: drawn on the frame the button is pressed.
- (from the attempt) Item sets come from `gSaveData->custom[col + row * 7].item[set - 1]`
  (97 entries of 0x38 bytes, per base character, 3 sets of 8 u16) or `gSaveData->rec[n]`.
- Original oddities: cancelling the stage phase returns a custom-list side to the wrong state.
