# Graphics

Sources: `src/sys/dma.c`, `gfx.c`, `gfx_ot.c`, `fade.c`, `vu1_packet.c`. Layouts and register
macros: `include/sys/dma.h`, `gfx.h`, `gfx_ot.h`, `fade.h`, `vu1_packet.h`.

`gfx_ot.c` and `vu1_packet.c` each have one function still in assembly, and their matches are
not linked yet at the time of writing.

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
