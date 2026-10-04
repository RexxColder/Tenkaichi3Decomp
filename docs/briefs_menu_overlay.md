# Shared brief: the menu overlay DBZP.BIN (tenth batch)

Read docs/agent_rules.md and docs/decomp_guide.md first (all of the guide, including the
"Matching lessons" sections at the end), then docs/game_overview.md (what it says about
DBZP.BIN and the progress-mode dispatcher), docs/systems/menu_support.md,
docs/systems/save_data.md, docs/systems/graphics.md (dialog, text printer, movie data) and
docs/systems/boot_and_frame.md.

The overlay: disc/BIN/DBZP.BIN, loaded at 0x00334C00, code 0x334C00..0x3B0E04 (737 functions),
data from 0x3B0E80 (file offset 0x7C280). It shares the main executable's `$gp` (0x304270) and
is compiled like the game code (ee-gcc 2.96, -O2 -G8). It is the whole front end: a dispatcher
on `gProgress->mode` (values 1..70, jump table at 0x3B1130) whose handlers each run one screen
(title, menus, character / stage select, options, shop, story, tournament, replay menu, ...)
and call the main executable for everything shared: `Dialog_*`, `McFlow_*`, `GetWin_*`,
`MsgWin_*`, `IconWin_*`, `ChrView_*`, `TextBox_*`, `ChrGrid_*`, `StgGrid_*`, `Num_*`,
`FlashAnim_*`, the Flash-like movie player, `Font_*`, `Snd_*`, pads, files.

Disassembly snapshot to read from (NOT the live asm/ folder):
/tmp/claude-1000/-home-z3-Desktop-decomp-bt3/ed9e766e-dea6-4684-86f5-14b7166075c4/scratchpad/snap3/asm/dbzp/000000.s
(one file; data in .../snap3/asm/dbzp/data/07C280.data.s). Main-executable names: look
addresses up in config/symbols/*.txt (`grep -rn "0x0025CFC0" config/`); the snapshot predates
most of them, so a `func_XXXXXXXX` there usually has a real name now, and the linked source in
src/ says what it does. Read the callee's source before guessing at a call's meaning.

Rules of the road: as in docs/briefs_remaining_main.md (new files only; no edits to existing
files; no ninja / configure / git; fdiff to verify; compare emitted .rodata / .lit4 / .sdata
bits with the original; check call targets by address; whole range in address order;
INCLUDE_ASM with a behaviourally exact attempt in `#if 0` for what resists; every function
named, guesses marked; split files at module boundaries and say where and why).
Files go in src/menu/<stem>.c, include/menu/<stem>.h, config/symbols/<stem>.txt.

Report as agent_rules.md asks, plus: which `gProgress->mode` values your screens are and how
each screen flows (states, what it loads, which mode it sets next); the data structures and
tables; file ids loaded and asset formats parsed; what is read from / written to `gSaveData`
and `gProgress`; how the battle is set up from the menu (anything written to the battle setup
is the hand-off to the simulation: document it exactly); random draws and their generator;
pad reads; original bugs; object-boundary evidence for the integrator.

## Chunks (function-boundary cuts; end exclusive)

| # | Range | Functions | Stem |
|---|---|---|---|
| 1 | 0x334C00..0x339610 | 25 | menu_a |
| 2 | 0x339610..0x33E108 | 22 | menu_b |
| 3 | 0x33E108..0x342588 | 29 | menu_c |
| 4 | 0x342588..0x348710 | 12 | menu_d |
| 5 | 0x348710..0x34D368 | 19 | menu_e |
| 6 | 0x34D368..0x351C38 | 3 | menu_f |
| 7 | 0x351C38..0x356090 | 22 | menu_g |
| 8 | 0x356090..0x35A558 | 41 | menu_h |
| 9 | 0x35A558..0x35F650 | 34 | menu_i |
| 10 | 0x35F650..0x364358 | 14 | menu_j |
| 11 | 0x364358..0x368C18 | 16 | menu_k |
| 12 | 0x368C18..0x36DBE8 | 30 | menu_l |
| 13 | 0x36DBE8..0x372148 | 17 | menu_m |
| 14 | 0x372148..0x376920 | 27 | menu_n |
| 15 | 0x376920..0x37AFF8 | 26 | menu_o |
| 16 | 0x37AFF8..0x37F430 | 30 | menu_p |
| 17 | 0x37F430..0x3840E0 | 33 | menu_q |
| 18 | 0x3840E0..0x388618 | 18 | menu_r |
| 19 | 0x388618..0x38CB38 | 27 | menu_s |
| 20 | 0x38CB38..0x3911A8 | 32 | menu_t |
| 21 | 0x3911A8..0x395E30 | 17 | menu_u |
| 22 | 0x395E30..0x39A978 | 28 | menu_v |
| 23 | 0x39A978..0x39EFC0 | 14 | menu_w |
| 24 | 0x39EFC0..0x3A3848 | 14 | menu_x |
| 25 | 0x3A3848..0x3A7D98 | 43 | menu_y |
| 26 | 0x3A7D98..0x3AC440 | 68 | menu_z |
| 27 | 0x3AC440..0x3B0E04 | 76 | menu_za |
