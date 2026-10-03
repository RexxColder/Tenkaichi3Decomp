# Open questions

## Build and layout

- The C files reference their global variables as `extern` from assembly; none defines its own
  data yet.
- Original source-file boundaries are mostly unknown. Evidence so far: `.rodata` alignment (the
  object containing the battle sequence starts at 0x215540), and delay-slot behaviour that only
  matches when certain accessors are not defined earlier in the same file (`battle_work.c` vs
  `battle_load.c`).
- The CPU player's code is two objects. The second one's file-scope tables (0x2EDA70..0x2EDF08)
  sit between the two groups of function-local data, which puts the boundary after
  `BtlAiStep_Unk23` (ends 0x1B6B08) and before `BtlAiCond_React` (0x1B7188). Nothing narrows it
  further: the functions in between emit no read-only data and link identically on either side.
  The files are split at 0x1B6D00, where the rule conditions' helpers begin. The first object is
  taken to start at 0x1B4140 (its first read-only item is the jump table at 0x2ED8C0); the data
  would equally allow an earlier start. Where the second object ends is unknown (at least
  0x1BA308).
- The 42 functions at 0x2BD230..0x2BF6B0, after the libraries, are game code (memory-card menu
  UI) that never uses `$gp`. Why they sit there, and whether they were built with different
  flags, is unknown.
- Two functions in the main body reach short `.sdata` strings with absolute addressing
  (0x1094A8..0x111358), suggesting objects built with different small-data flags.
- Assembler prelude gaps (see decomp_guide.md): a load, store or `la` between a compiler-filled
  delay slot and an unfilled branch; `li.s` under `-G0`.

## Functions that resist matching

Still pulled from assembly inside linked files:

| Function | File | What differs |
|---|---|---|
| `BtlInput_Update` | `btl_input.c` | 6 of 149 instructions: the emission order of twelve stores |
| `DemoCam_Update` | `btl_demo_cam.c` | 11 of 384: two saved registers swapped in one branch |
| `BtlText_DrawScrollBar` | `btl_seq.c` | one register swap |
| `Ot_Reset` | `gfx_ot.c` | the original copies an address through two extra registers |
| `Vu1Node_Animate` | `vu1_packet.c` | one instruction short; different loop induction variables |
| `BtlScene_TestCharPackBit`, `BtlScene_IsEffectHidden` | `btl_scene.c` | a saved-register choice; a switch's shared block |
| `IopHeap_PrintFree` | `iop_heap.c` | the original saves an unused register and does not tail-call |
| `Movie_ReadBuf` | `movie.c` | 5 of 44: order and register choice |
| `Sprite_DrawPicture` | `sprite.c` | 12 of 150: scheduling in the row loop |
| `Rigid_Init` | `rigid.c` | 2 of 72: two instructions swapped before a memset |
| `PadWatch_GetMissing` | `pad_watch.c` | 2 of 69: delay-slot fill |
| `ChrCam_CalcCut` | `btl_char_cam_cut.c` | 248 of 565: register allocation in the four "resolve a node" blocks; it owns two `.lit4` constants, which is why the fighter camera is three files |
| `BtlAiStep_Unk17` | `btl_ai_seq.c` | 63 of 154: branch layout of the first half, registers of the class comparisons |
| `BtlAiCond_TypeRateByOppAction` | `btl_ai_cond.c` | 5 of 50: a result register and the place of two pointer adds |
| `BtlAi_GetPairRate`, `BtlAi_GetQuadRate` | `btl_ai_cond.c` | 75 of 90: the original repeats the row code in every even case and shares one tail for the odd ones |

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
