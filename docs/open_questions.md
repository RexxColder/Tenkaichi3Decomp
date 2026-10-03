# Open questions

## Build and layout

- The C files reference their global variables as `extern` from assembly; none defines its own
  data yet.
- Original source-file boundaries are mostly unknown. Evidence so far: `.rodata` alignment (the
  object containing the battle sequence starts at 0x215540), and delay-slot behaviour that only
  matches when certain accessors are not defined earlier in the same file (`battle_work.c` vs
  `battle_load.c`).
- The 42 functions at 0x2BD230..0x2BF6B0, after the libraries, are game code (memory-card menu
  UI) that never uses `$gp`. Why they sit there, and whether they were built with different
  flags, is unknown.
- Two functions in the main body reach short `.sdata` strings with absolute addressing
  (0x1094A8..0x111358), suggesting objects built with different small-data flags.
- Assembler prelude gaps (see decomp_guide.md): an FPU instruction directly before an unfilled
  return; `li.s` under `-G0`.

## Functions that resist matching

Still pulled from assembly inside linked files:

| Function | File | What differs |
|---|---|---|
| `BtlInput_Update` | `btl_input.c` | 6 of 149 instructions: the emission order of twelve stores |
| `DemoCam_Update` | `btl_demo_cam.c` | 11 of 384: two saved registers swapped in one branch |
| `OrbitCam_Reset` | `orbit_cam.c` | prelude gap: a float load before a call is not moved into the delay slot |
| `BtlText_DrawScrollBar` | `btl_seq.c` | one register swap |
| `Ot_Reset` | `gfx_ot.c` | the original copies an address through two extra registers |
| `Vu1Node_Animate` | `vu1_packet.c` | one instruction short; different loop induction variables |
| `BtlScene_TestCharPackBit`, `BtlScene_IsEffectHidden` | `btl_scene.c` | a saved-register choice; a switch's shared block |
| `IopHeap_PrintFree` | `iop_heap.c` | the original saves an unused register and does not tail-call |
| `Movie_ReadBuf` | `movie.c` | 5 of 44: order and register choice |
| `Sprite_DrawPicture` | `sprite.c` | 12 of 150: scheduling in the row loop |
| `Rigid_Init` | `rigid.c` | 2 of 72: two instructions swapped before a memset |
| `PadWatch_GetMissing` | `pad_watch.c` | 2 of 69: delay-slot fill |

Matching per function but not linked (in `pending/`):

| File | Why |
|---|---|
| `btl_ai.c` (51 of 55 match) | spans two original objects; its read-only data only lines up if each part starts where the original object did, which means pulling in the earlier functions |
| `btl_char_cam.c` (21 of 23 match) | one function is blocked by the prelude gap (float load before a call); the unmatched cut evaluator owns constants in the middle of the pool, so the file must be split in three |

## Game structure

- What each of the 70 `gProgress->mode` values (overlay dispatch) is.
- Which game modes battle modes 2..5 correspond to in the menus; where modes 8 and 9 come from.
- The meaning of most battle event ids, of "character flag 7" and character flag 0x128.
- The save block's `slot[9]`, `unlockFlags`, `rule[6]`, and its large unidentified ranges.
- What sets the per-side voice mute (`func_00259E20`).
- What the three unanalysed boot inits do (`func_00116BA8`, `func_00239FF0`, `func_0023D0E0`).
- Whether further code is ever loaded from the archives (only one overlay table entry exists).

## Not started

- The fighter code: actions, state machine, hit detection, AI.
- The stage, effects, HUD and camera internals.
- The 3D renderer, the model/texture/animation formats and the nine VU1 microprograms.
- The VU0 vector library (about 223 functions).
- The memory card module and the movie player.
- The whole menu overlay (737 functions).
- `SOUNDS.IRX` (the sound driver) and the bank format.
- The archives: nothing has been extracted or catalogued.
- The Wii build's netcode.
