# Graphics

Sources: `src/sys/dma.c`, `gfx.c`, `gfx_ot.c`, `fade.c`, `vu1_packet.c`. Layouts and register
macros: `include/sys/dma.h`, `gfx.h`, `gfx_ot.h`, `fade.h`, `vu1_packet.h`.

`gfx_ot.c` and `vu1_packet.c` each have one function still in assembly.

## Display lists (verified; purposes inferred from hardware addresses)

- Two 1 MB display-list buffers; `gDmaCur` is the write cursor. `Dma_Flush` appends FINISH and an
  END tag, sends on VIF1 and swaps buffers.
- `Dma_AddData` copies a ready packet; `Dma_AddRef` / `Dma_AddNext` write chain tags;
  `Dma_BeginDirect` / `Dma_EndDirect` bracket a packet built in place (and flush the cache on
  every packet).
- Register packets, one function each: tex flush, frame, clamp, scissor, xy offset, zbuf (always
  24-bit), tex0, tex1, fba, colclamp, alpha.
- Rectangles and textured quads are drawn as strips of 32-pixel-wide sprites
  (`Dma_AddFillRect`, `Dma_PutTexStrips`).

## Frame buffers (verified)

- Two 512x448 32-bit colour buffers (GS pages 0x00 and 0x70) and one shared 24-bit depth buffer
  (page 0xE0). Larger Z is nearer; the clear writes Z = 0.
- XYOFFSET (1792, 1824); scissor 0..511 by 0..447.
- NTSC interlaced, field mode. `Gfx_Init` first builds a throwaway 640x448 environment only to
  clear video memory at boot.
- Buffer selection uses bit 0 of the frame counter (`gGfx + 0x234`); `gGfx + 0x230` is the
  field parity flag.
- Display: both read circuits show the same buffer one line apart, blended 50/50 (a flicker
  filter; PMODE 0x7F23). MAGH 5 stretches 512 to 2560. `DX = 636 + 4 * screenX`,
  `DY = 50 (51) + 2 * screenY`, with the offsets taken from the save.

## Default render state (verified)

`Gfx_AddDefaultEnv` / `Gfx_PutDefaultEnv` write, for context 1: TEXA TA1 = 0x80; default offset
and full-screen scissor; FRAME = current buffer; TEST 0x50000 (depth test on, GEQUAL, no alpha
test); FBA 0; ZBUF with writes on; ALPHA 0x44 (standard blend); CLAMP clamp/clamp;
PRMODECONT 1; COLCLAMP 1; TEX1 0x60 (bilinear).

`Gfx_ClearScreen(fbmsk, rgba)` fills 512x448 with Z writes off.

## Ordering table (verified layout and order; depth direction inferred)

- 0x1002 slots (a first slot, 0x1000 depth slots, a last slot), two layers each. Each chain
  starts with a head packet setting the blend: layer 0 is 0x44 normal / 0x42 subtractive,
  layer 1 is 0x48 additive / 0x49.
- Packets come from a 0x73000-byte pool.
- `Ot_Draw` links the non-empty chains in table order, sends an environment packet, all the
  data, then a restore packet, and resets the table.
- Low slot index = far (painter's order) is inferred. The table's init/term are called from the
  battle side, so it appears to be battle-lifetime.

## Battle draw passes (verified order)

`Gfx_MarkPass(n)` is an empty debug marker. Order in `Battle_Draw`: stage (0x115950, 0x115DE0);
0x247578; 0x10FF40; 0x247660; default env + scene manager draw (0x12CCD0) + `Ot_Draw`; 0x247688;
HUD (`BtlGame_Draw`); 0x23A2B8; 0x2476D8. In split-screen only the stage pass and the scene
manager pass are repeated per view; everything else is drawn once. What the 0x247xxx passes draw
is weakly supported ("effects").

## Fades (verified)

`gFade[3]`, 0x60 bytes each: current colour, from, to, delta (four floats each), a ramp, flags
(1 fade-out started, 2 fade-in started, 4 done, 8 paused).

- Slot 0 is black; slots 1 and 2 are white. Alpha runs 0..128. The colour belongs to the slot.
- `Fade_Start(slot, dir, seconds)`: dir 0 covers the screen, dir 1 uncovers it.
- A finished fade-out stays opaque until reset or faded in; a finished fade-in turns itself off.
- `Fade_IsDone` returns 1 one frame before the final colour is computed, and also for a slot
  that is off.
- Slots 0 and 1 are drawn by `Gfx_EndFrame`; slot 2 is drawn from the battle side (inferred: the
  white flash on a stage change).
- `Fade_Init` only clears slot 0; the others rely on zeroed memory.

## VU1 packet layer (verified code, guessed names)

Nine VU1 microprograms sit right after the code (0x2BF6B0..0x2C3380), numbered 0..8 by address;
what each draws is not known. `Vu1Pkt_LoadProgN` uploads a microprogram and its constant block;
`Vu1Pkt_CallProgN` builds the call packet in front of an object's vertex chain. `Vu1Node_*`
walk a model node list. Callers are the 3D renderer at 0x111358..0x115DE0 (not decompiled).

## For a port

A renderer needs: 512x448 internal resolution, a greater-or-equal depth test with larger Z
nearer, bilinear filtering by default, four blend modes selected by ordering-table layer, and
painter's-order drawing through the table. The flicker filter and the field handling can be
dropped. The model, texture and microprogram formats are not understood yet.

## Movies (`src/sys/movie.c`; verified unless marked)

- A movie is two loose files opened by name, not archive entries: `zs3usop.pss` + `.adx`
  (opening) and `zs3used.pss` + `.adx` (ending). Only the video stream of the `.PSS` is used.
- Path: disc read (0x100 sectors) into one of four buffers, demux with Sony's libmpeg into a
  ring buffer, decode one picture, and upload it straight into the frame buffer as 896
  transfers of 16x16 blocks. The movie is never a textured quad.
- One picture per two vertical blanks. There is no time-stamp sync with the audio: if the
  player waits on the disc for more than four fields it pauses the audio and resumes it after.
- START on pad 1 skips with a 0.5 s fade; pad 2 is not read.
- Buffers (about 3.8 MB) come from the main heap and are freed on close.
- `Movie_PlayEnding` has no direct callers in either binary.
- For a port (inferred): decode the video with any MPEG-2 decoder at 512x448, play the ADX
  from the first frame, and sync video to the audio clock.

## 2D sprites (`src/sys/sprite.c`; verified code, format partly inferred)

A sprite sheet is a BPE-packed file whose word at +0x10 points to 0x40-byte texture entries.
`Sprite_DrawList` draws `LoadSprite` records as blended GS sprites; each quad uploads its
texture first, so one texture is resident at a time. The pixel layout belongs to the uploader
`func_0010A218`, not decompiled.

## Colour overlay (`src/sys/color_fade.c`; verified)

A fourth screen overlay besides the three fade slots: an integer alpha 0..0x80 stepped per
update, drawn as one blended 512x448 sprite. Used only by the trailing memory-card block.

## Controller-removed overlay (`src/sys/pad_watch.c`; verified)

`PadWatch_Update` and `PadWatch_Draw` run every frame from `Gfx_EndFrame`: presence of both
pads is debounced (about 7 frames) and a dimmed screen with a message from the boot file is
drawn when a required pad is missing. Loading screens disable it. It also pauses the battle;
see netplay_notes.md.

## Text printer and button icons (`col_c.c`, `col_c_b.c`, 0x239EA0..0x23D1E8; verified unless marked)

122 of 124 functions match per function; not linked yet. The source file really starts at
0x239BB0 (five helpers at the end of the neighbouring col_b file). Drawing only: no random
draws, no pad, camera or sound input.

- **Queue model**: `Font_Print*` only queue a 0x54-byte command with a copy of the current
  style (120 commands at most, no bound check). `Font_Flush(first)` draws commands
  `first..count` as one GS packet and truncates the queue; `Font_FlushAll` is `Font_Flush(0)`.
  Strings are stored by pointer, so they must stay valid until the flush. Caller idiom:
  `n = Font_GetCmdCount(); Font_PushStyle(); ...; Font_PrintAt(...); Font_PopStyle();
  Font_Flush(n);`.
- **Command kinds**: 16-bit string, 8-bit string, single character, decimal number with a fixed
  digit count. Only 16-bit strings expand tags.
- **Style** (0x48 bytes, five-deep stack): scales, colour, icon colour, spacing, alignment per
  line, flags, shadow mode (1 one copy, 2 eight copies, 3 four copies), clip box, callbacks.
- **Fonts**: seven slots; the boot file's two fonts go to slots 0 and 1. Glyph lookup is a
  binary search over 6-byte records; glyph cells are at most 31 x 31 texels.
- **Tags in text**: `<PAD=names>` draws controller button icons, `<PADS=a,b,c,d>` one list per
  controller type (animated), `<COL=RRGGBBAA>` / `<COL=DEF>` changes or restores the colour,
  `<UB0>` a special character. The icon animation clock (`FontIcon_Tick`) is advanced from
  `Gfx_EndFrame`, once per 30 Hz frame.
- Corrections: `func_0023A458` / `func_0023A2D0` (used in pad_watch.c, btl_seq.c) are
  `Font_GetCmdCount` / `Font_Flush`; the "unidentified inits" at boot, `func_00239FF0` and
  `func_0023D0E0`, are `Font_Init` and `FontIcon_Init`; `func_0023D160` is `FontIcon_Tick`.

## Confirmation dialog (0x268248..0x269228; src/sys/lib_a.c, not linked yet; names in config/symbols/lib_a.txt)

A two-choice confirmation window ("Yes / No"), one instance, `gDialog` at 0x2FF160 (0x8C bytes
on heap 2). 18 of 19 functions match; `Dialog_SetCursor` differs in registers only. The
`Dialog_` prefix is our choice. Proper file name: `src/sys/dialog.c`.

- (verified) It is a UI animation object with clips `mc_dummy_text_1..4` (text anchors) and
  `mc_menu_plate_1/2` (the two choices), and labels `fl_window_{s,l}_{in,out,open}`,
  `fl_on_start`, `fl_off_start`, `fl_ok`. Struct layout in include/sys/lib_a.h.
- (verified) `Dialog_Input` returns 1 for the first choice, -2 for the second, -1 for cancel,
  0 otherwise. Pad: game-button repeat bits 1 / 2 move the cursor, pressed 0x200 confirms,
  pressed 0x400 cancels. Blocked while `gProgress->flags & 0x100`, while the window is not
  fully open, or when there are no choices.
- (verified) Sound effects, `Snd_PlaySe(1, n)`: 0 cursor, 1 confirm, 2 cancel, 4 open, 5 close.
- (verified) Message tables are `u32` offset tables: text i starts at
  `table + (table[i+1] & ~3)`, UTF-16 strings for the Font module.
- (verified) Resource file: a table of byte offsets; words 1, 4, 2, 3 are texture parts
  (relocated with `Res_RelocateOffsets`), word 5 the animation data, word 6 the title table.
- (inferred) Its callers at 0x119FB8..0x11E1E8 are the memory-card / save confirmation flows.
- Original oddities kept in the matching C: `Dialog_DrawBody` reads its text from the title
  table, not the caller's message table; `Dialog_Term` uses the animation before its NULL
  check.
- For linking: `gDialog` must be defined as initialised data (it sits in `.sdata` before
  `"fl_ok"`); `.rodata` at 0x2F35B0, size 0x145; `Snd_PlaySe` must be declared `void` here.

## Post-process passes (0x102F28..0x106D60; src/sys/gfxm_a.c, not linked yet; names in config/symbols/gfxm_a.txt)

46 functions, 42 match; `GfxPost_DrawGlow`, `StgPanBlur_DrawView`, `StgPanBlur_UpdateView` and
`StgDepthTint_Draw` are INCLUDE_ASM with attempts (instruction-level differences only). The
module names are readings of the technique, not confirmed on screen. Layouts in
include/sys/gfxm_a.h. Suggested final name: sys/gfx_post.c (with gfxm_b).

- (verified) Post effects read the depth buffer page (0xE0, texture block 0x1C00) as an 8-bit
  texture of its spare top byte (a per-pixel object id / mask), through 256-entry CLUTs built
  at run time. CLUT blocks: 0x3E98 pan blur, 0x3E8C outline, 0x3E88 glare, 0x3E64 depth tint,
  0x3E90 object glow. Work pages 0x150 (texture block 0x2A00) and 0x170 (0x2E00); (inferred)
  0x1F6 / 0x1F8 (64x64) for the glow pass.
- Modules: pan blur (`StgPanBlur_*`, stage feature 0xD; strength built from the camera's
  horizontal movement per frame), glow pass (`GfxPost_DrawGlow`: reduce to 256 / 128 / 64,
  blur, add back), packet helpers (`GfxPost_*`), object outline (`ObjOutline_*`, called by
  `BtlObj_InitDraw / TermDraw / BeginDraw`), sky glare (`StgGlare_*`, stage section +0x48),
  depth tint (`StgDepthTint_*`, stage section +0x40; index 1 when the camera is under the
  water level, never in split screen), object glow (`ObjGlow_*`, per-id alpha table from
  `BtlObj_UploadAlphaTable`).
- (verified) `BtlObj_BeginDraw` runs the outline pass, then the object glow.
- (verified) Stage parameter formats: glare = s32 flags (1 enabled, 2 track), s32 step, u8 max,
  hold, min, alpha (defaults: off, alpha 0x40, step 0x20, max 0x80, hold 0x50, min 4); depth
  tint = 0x10-byte entries: s32 flags (1 enabled, 2 additive, 4 clear entry 0xFF), u8 r, g, b,
  a, three (x, z) byte pairs of curve keys.
- No pad, clock or random draw; reads the camera and `Battle_IsSplitScreen`;
  `BATTLE_FLAG_PAUSE` freezes the pan blur update. Nothing feeds back into the simulation.
- For linking: `GfxPostQuad` duplicates `GfxQuad` of include/sys/gfxm_b.h; stg_c.c / btl_obj.c
  declare some of these with other argument types (the definitions here are what match).

## Screen passes, texture files, movie data (0x106D60..0x10AD58; src/sys/gfxm_b.c, gfxm_b_b.c, gfxm_b_c.c; not linked yet; names in config/symbols/gfxm_b.txt)

63 functions, 53 match; 10 are INCLUDE_ASM with attempts (`GfxPost_DrawTintRect`,
`GfxLens_DrawAll`, `GfxLens_PutCapture`, `GfxWater_DrawView`, `GfxWater_Draw`,
`GfxPost_ShiftHighWord`, `TexChain_Build`, `TexChain_BuildPair`, `GfxClut_InitPacket`,
`Flash_SkipNamed`). Layouts in include/sys/gfxm_b*.h. Suggested final names: gfx_post_b.c /
gfx_lens.c / gfx_water.c / gfx_depth_fog.c, tex_file.c, flash_data.c.

Depth-buffer tricks (verified):
- The depth buffer is 24-bit (page 0xE0, block 0x1C00); its spare top byte is an 8-bit scratch
  channel. `GfxPost_CopyAlphaToDepth` copies the colour buffer's alpha into it (every frame,
  from `BtlObj_BeginDraw`). `GfxPost_DrawDepthClut(mode, tbp, cbp, alpha)` draws that byte
  through a 256-entry CLUT over the screen (16 strips of 32x448).
- Depth fog (`GfxDepthFog_*`, name inferred): CLUT black with alpha 255 - index, indexed by the
  high depth bits. Alpha key (`GfxAlphaKey_*`, purpose inferred): pixels whose frame alpha is
  0xF4..0xFE get a wash of one of eleven fixed colours.
- Lenses (`GfxLens_*`, use unknown): 8 slots that distort a captured copy of the screen around
  a world point; nothing in the decompiled code starts one.
- Underwater wobble (`GfxWater_*`): when the camera is under the stage's water level the view
  is redrawn through a 6 x 10 grid displaced by two cosine waves (speeds 0.06 / 0.04 per
  frame), tinted by the stage's water colour. (from disassembly) original bug: the border test
  uses row index 9 on a 6-row grid, so only the top row is pinned.

Texture files (verified): count at +0, entry-table offset in words at +4, 0x40-byte entries
(`TexEntry`: pixel / CLUT offsets, sizes, block steps, BITBLTBUF bits, TEX0, pointers);
`Res_RelocateOffsets(&file, base, hdr)` returns nothing. Uploads queue the 0x30-byte packet at
0x2C3410 then a DMA REF to the entry's ready packet. `TexFile_UploadOne(file, index, tbp,
cbp)` (0x10A218; btl_obj.c's older declaration and comment are wrong), `Tex_Upload` (0x10A288;
arguments are block pointers, not x / y), `TexFile_UploadAll` (0x10A480), `Tex_Log2Size`
(0x109F50), `GfxClut_InitPacket` (0x10A5A0: a complete 256-entry CLUT upload packet, CLUT
bytes at +0x70).

Movie ("Flash-like") data (verified code, format names inferred): header bytes 'F' 'O' 'D'
0x11 and the string "LIT"; a list of 8-byte tag headers (code u8, record count u16 +2, size
u32 +4), code 0 ends; named blocks (NUL-terminated name, u16 size, data); a bit reader, most
significant bit first; `Flash_ReadMtx` reads the SWF MATRIX record with an explicit flag byte
(scale and skew / 65536, translation in twentieths).

No pad, clock or random draw anywhere in the range; the lens and water passes read the camera
and the split-screen mode. Nothing feeds back into the simulation.
