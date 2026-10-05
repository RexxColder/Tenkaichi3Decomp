# PC port: working notes

This repository is the PC port. It was cloned from the matching decompilation at tag
`port-base-2026-10-05`; the decompilation stays a separate repository and is the remote
`decomp` here. `git fetch decomp && git merge decomp/master` brings in later matching work.

## Rules of the layout

- `src/`, `include/`, `config/`, `scripts/`: the decompilation's files. Do not edit them here.
  When a game source needs a change to build on PC, make it in the decompilation behind
  `#ifdef PORT`, check there that the matching build is still byte-identical, then merge.
- `port/`: everything that exists only for the PC build (compatibility header, platform
  layer, tools, build files). `port/build/` is output and is ignored.
- `port/include/port_compat.h` is forced into every game source (`-include`). It defines
  `PORT`, takes over the base types (`long` is 64-bit on the PS2, so `s64 / u64` become
  `long long`) and turns the assembly-inclusion macros into nothing.

## State on 2026-10-05 (first day)

Verified by running the tools below:
- All 204 game sources of the main executable and all 69 menu-overlay sources pass the host
  compiler's front end as 32-bit code (`port/tools/survey.py`). Three `#ifdef PORT` guards
  were needed: the 128-bit typedefs (include/sys/dma.h), one 128-bit store (Dma_BeginDirect),
  and five cast macros that are assigned to.
- 196 of 204 compile to objects (`port/tools/undefined.py`). The eight that do not contain
  MIPS inline assembly: dma.c, file.c, mathf.c, randf.c, vu0_a_c.c, vu0_a_c_b.c, vu0_a_c_c.c,
  vu0_b_c.c (the vector library has exact C references in src/port/).
- What a link of those 196 objects still needs (list in port/build/undefined.txt):
  423 data symbols (data the decompilation still keeps in assembly chunks: to be extracted
  from the original executable by a tool, the user's own copy, not committed), 179 game
  functions (the INCLUDE_ASM functions whose C sits in `#if 0`, the hand-written assembly
  routines, and the functions of the eight files above), 69 library functions (Sony SDK,
  CRI sound, libc: the platform layer), 12 others (menu-overlay entry points and host
  compiler helpers).

## Decisions so far

- 32-bit host build first (`-m32`): the game stores pointers in 32-bit fields everywhere.
  A 64-bit build (needed for Android) is a later clean-up, not a blocker for validation.
- First milestone (docs/roadmap.md stage 2): the fight simulation headless, fed by a replay
  recorded on the original, compared frame by frame.

## State at the end of 2026-10-05

Tools (run from the repository root, in this order): `port/tools/gen_data.py` (the game's
remaining assembly data as host assembly, from the decompilation's generated asm/),
then `port/tools/undefined.py` (compiles everything to port/build/obj and lists what a link
still needs in port/build/undefined.txt). `port/tools/portsrc.py` is their shared module: it
lists the sources of the PC build and switches on the C of functions that the matching build
still takes from assembly.

Verified by running them: 202 of 204 game sources of the main executable build to 32-bit
objects (the four vu0 files are replaced by src/port/vu0_a.c + vu0_b.c compiled under the
game's names through port/src/vu0_names.h). Still failing: mathf.c and randf.c (COP1 / VU0
assembly: need PC versions on the PS2 float model, including the VU0 random register).
A link still needs: 21 data symbols (VU1 microprograms and a few tables), 39 game functions
(mathf, randf, three StgVu_Rotate routines, ObjSeam_TransformVtx, the entry point, and
library names in the game range), about 190 library functions (Sony SDK, CRI, libc; many are
referenced only by library data tables that the PC build can drop).

NOT verified: that every vector-library reference has the same parameter list as the game's
callers expect (the callers declare these functions locally); the maths harness will show it.

## Update 2026-10-05 (late): every game source builds

All 204 game sources of the main executable build to 32-bit objects. mathf.c / randf.c: their
five assembly functions (Mathf_WrapAngle, Mathf_SinFast, Mathf_Sqrt, Rand_SeedFloat,
Rand_Float01) are guarded by `#ifndef PORT` and supplied by port/src/mathf_pc.c, operation
by operation on the PS2 float model; the VU0 random register is `gPortVu0R` (simulation
state: it must be saved and restored with the rest for netplay). Sanity-checked only
(sine within 0.002 of libm, square roots, value range of the generator), not against a console.
Game routines still missing: StgVu_RotateX / Y / Z, ObjSeam_TransformVtx (hand-written VU0
assembly outside the vector library) and the entry point.

Open points for bit-exact simulation, none solved yet:
- The game's ordinary C float arithmetic is compiled natively here (IEEE, round to nearest);
  the PS2 truncates and has no denormals / infinities. Decide: software float for simulation
  code, or prove where it matters.
- libm (sinf, tanf, asinf, acosf, atanf, atan2f) and libc rand() are the SDK's newlib
  versions on the PS2; the host's give different bits. The PC build needs its own exact copies.

## First link (2026-10-05, late)

`port/tools/link.py` links all objects with generated stubs for the 238 symbols nobody
provides yet (a stub prints its name and exits) into port/build/bt3, a 32-bit Linux
executable. It links without duplicate or undefined symbols and runs the game's own `main`
up to its first SDK call (`sceSifInitRpc`). C library and libm calls go to the host for now.
Build order: gen_data.py, undefined.py, link.py. Running the executable and implementing
whatever stub it names next is the working loop for the platform layer.
The symbols game code itself needs (not only library data tables) are about 130: CRI ADX
(31), Sony file / CD / SIF / kernel (about 35), GS and MPEG (about 20), pad and vibration
(8), libc / libm (about 25), the nine VU1 microprograms and a few tables, the menu overlay's
entry points (the overlay is not part of this link yet), four VU0 assembly routines.

## Start-up runs (2026-10-05, night)

Verified by running port/build/bt3: `Game_Main` completes its whole initialisation list
(Sys_RebootIop ... PadWatch_Init: heap, graphics, sound, files, movie, common data, save,
jobs, vector unit, DMA buffers, pad, memory card, fonts, fade) and stops at `Overlay_Load`,
where the PS2 reads the menu overlay to a fixed address. Files come from loose folders.

Platform layer so far (port/src):
- plat_mem.c: memory at PS2 addresses (heap 0x00400000..0x02000000 served to the game's one
  malloc; hardware register pages; scratchpad).
- plat_file.c: CRI ADXF over folders made by port/tools/extract_disc.py (`gamedata/`, or
  BT3_DATA); `gamedata/mods/<same path>` overrides a file.
- plat_gs.c: DMA channel control registers (a started transfer completes at the next access;
  the data is dropped in the headless build).
- plat_sys.c, plat_stub.c, plat_mc.c: system calls, sound, GS, pads, movies (do nothing),
  memory card ("no card").
- port/tools/where.sh: where the program is stuck or crashed.

Changes this needed in the shared sources (all made in the decompilation, matching build
unchanged): DMA control register macros go through `Port_DmaChcr` under PORT; the 68 integer
literals with an `L` / `UL` suffix became `LL` / `ULL` (`1UL << 34` is wrong on a 32-bit
host); guards around five assembly maths / random functions and three hardware waits.

Not looked at yet: other uses of the PS2's 64-bit `long` semantics that are not literals
(none found by grep: the sources use s64 / u64), struct layouts that depend on 64-bit
alignment (u64 members are 8-byte aligned on the PS2 and 4-byte aligned on 32-bit x86: must
be checked, `-malign-double` is the likely fix), and float behaviour.

## The fight loop starts (2026-10-05, night)

port/src/headless.c stands in for the menu overlay (`Progress_Main`): it sets up the game's
own attract-demo battle (CPU against CPU), or loads a replay save file named by BT3_REPLAY
(0x1AC00 bytes) through `BattleReplay_Load`, and ends the program when the battle is over.
Verified by running it: the demo battle loads completely (sound banks, stage, both character
models and parameter files, from loose files) and `Battle_Loop` runs; 186 vertical blanks in,
the first CPU update crashes in `BtlObjAnim_QueryEvent`.

Cause, and a general lesson: PROTOTYPE DISAGREEMENTS. Game sources declare the functions
they call locally. On the PS2 `s32` against `u64`, the order of float against integer
parameters and "struct by value" against pointer all produce the same code; on 32-bit x86 the
stack layout differs. `port/tools/check_protos.py` compares every declaration with the
definition by ABI class: 127 of 14,887 declarations disagree (86 functions; list in
port/build/protos.txt). They are being fixed in the decompilation. The same check shows the
vector-library references agree with all their callers except `Vec3_ScaleAdd`.

Other checks added: `port/tools/check_layout.py` (14,862 documented PS2 member offsets
against the host compiler: all agree with `-malign-double`, 22 differ without it; one
remaining report, EftTask.unk4 in eft_c.h, not examined).

The headless build has no real-time pacing: a vertical blank happens whenever the game waits
for one (`gPortVBlanks` counts them).

## A recorded fight plays through (2026-10-05, late night)

Verified by running it: `BT3_REPLAY=gamedata/validation/replay01.bin port/build/bt3` (a replay
the user recorded in PCSX2: versus, character 0 on the pad against character 13 as CPU,
stage 0) loads, runs the intro states, the whole fight and reaches the battle sequence's last
state. Headless it takes a few seconds.

Comparison with the console (the user's PCSX2 save state taken at the result screen of the
same fight, gamedata/validation/state01/eeMemory.bin):

| | Console | PC build |
|---|---|---|
| address of the sequence object (`gBtlSeq`) | 0x187A420 | 0x187A420 |
| battle clock at the end | 0x83F ticks, 1:10.366 | 0x843 ticks, 1:10.500 |
| time left | 170 s | 170 s |
| pad fighter's health | 23050 | 23400 |
| CPU fighter's health | 0 (knocked out) | 7490 |
| heap bytes identical at the end | | 92.1 % of 28.5 MB |

So the heap layout is the console's, the fight follows the recording closely, and it DIVERGES
somewhere: the PC fight ends when the recording runs out, not by the knock-out. Not yet known
where it first differs. Known candidates, none examined: the host's libm (sinf, atan2f, ...)
and libc rand() in place of the PS2's; details of the float model (never checked against a
console); the unmatched functions' C; remaining PS2-only assumptions like the ones below.

What it took after the first link (lessons, each a class of PS2-versus-PC difference):
1. PROTOTYPE DISAGREEMENTS (check_protos.py): fixed in the decompilation, 127 declarations.
2. CALLS THAT RELY ON A LEFTOVER REGISTER: a callback called with no arguments whose target
   reads its first argument (`HudNode_Update`), a call with two arguments to a function that
   uses three (`Res_RelocateOffsets`). The checker cannot see calls through pointers; more
   may exist.
3. FLOAT SEMANTICS: with host floats the first battle frame produced NaNs (the PS2 has none)
   and an endless angle wrap. All game code is now compiled `-msoft-float -mno-sse -mno-mmx`
   and port/src/softfloat_ps2.c implements every float operation with the PS2 model
   (src/port/vu0_a.c): floats travel as bit patterns in integer registers, nothing of the
   host's float unit is used. port/src/plat_libm.c (the only hard-float file) bridges libm and
   double arithmetic. This also removed the 16-byte alignment faults (no SSE moves).
4. DATA THE C FILES TAKE FROM ASSEMBLY (INCLUDE_RODATA) and the VU1 microprograms: gen_data.py.
5. PAD: an all-zero pad packet means every button held (buttons are active-low): the headless
   pads are connected and idle.
6. `long` is 64 bits on the PS2: literals respelled LL / ULL in the decompilation.

Tools added: port/tools/try.sh (test uncommitted decompilation edits here), where.sh,
mc_extract.py (PS2 memory card image reader), BT3_TRACE=<n> (fight state every n vertical
blanks), BT3_DUMP=<file> (heap dump at the end of the battle, for comparison with a console
memory dump: both use the same addresses).

## Hunting the divergence (2026-10-05 / 06)

Console reference: four PCSX2 save states the user made while PLAYING BACK the replay
(gamedata/validation/play01..04: 55 frames into the countdown, battle ticks 319, 906, 1506)
and state01 (result screen of the original fight). Tools: `BT3_AT=<ticks>` dumps heap and
globals at those ticks; `port/tools/compare_state.py <tick> <eeMemory.bin>` compares the
heap byte by byte and the global variables by name (pointer-aware);
`port/tools/intro_metric.py` reads a `BT3_TRACE=1` trace and reports how close the PC is to
play01 (the fighters already move during the countdown, so this is the earliest check).

Verified:
- Health is identical at tick 319 (38030 / 26510) and different at 906 and later.
- A replay stores no random seed; the game's generators differ between any two runs
  (docs/netplay_notes.md in the decompilation), so they are not expected to match.
- 55 frames into the countdown the fighter positions already differ from the console in the
  last few bits (e.g. x 0x43119222 against 0x4311922A): the divergence is ARITHMETIC and starts
  at once; everything later is amplification.

Float model, as far as it is established (port/src/softfloat_ps2.c; PCSX2 is the reference
because the save states come from it):
- FPU add / subtract: truncating, and the smaller operand loses its low bits first (exponent
  difference d of 1..24 clears d - 1 bits, 25 or more leaves only the sign). CONFIRMED: with
  it a decaying animation value matches the console bit for bit at tick 319 (0xB5C0554B);
  with plain truncation it was 3 units low.
- FPU division: to nearest. Evidence: 1.0f / 30.0f in gFade.
- FPU multiply: truncating (assumed). Vector unit add / multiply / divide: truncating
  (assumed). Square root: the vector unit's truncates, the FPU's / libm's rounds to nearest
  (assumed).
- The integer routines agree with the host float unit on 4,000,000 random cases per
  operation in both rounding modes, so what remains is the choice of model, not its coding.
- libm is the PS2's own (newlib 1.10.0 float sources, unmodified, in
  port/third_party/newlib_libm, compiled with software float); libc rand is newlib's
  64-bit generator (read from the executable).
- Experiment switches (environment): BT3_VU_NEAREST, BT3_FPU_NEAREST, BT3_VU_ADDHACK,
  BT3_FPU_NOADDHACK. None of the six combinations tried makes the countdown positions exact
  (differences stay around 0.001 units summed over six coordinates).

Not known: which operation still differs. Candidates: a vector-unit operation PCSX2 treats
differently from the assumption, an instruction the original uses where our C compiles to
something else (multiply-add, the FPU square root's handling of edge cases), or a logic
difference that only shows in the last bits.
The decisive tool would be a small PS2 test program run in PCSX2 that prints the result bits
of every operation class for fixed inputs (the maths harness planned in docs/roadmap.md).

## The replay reproduces the console's fight (2026-10-05, evening)

Verified by running it, against the user's PCSX2 save states:

| Battle tick | Console health (pad / CPU) | PC build |
|---|---|---|
| 319 | 38030 / 26510 | 38030 / 26510 |
| 906 | 33880 / 16320 | 33880 / 16320 |
| 1506 | 26110 / 6970 | 26110 / 6970 |
| end | 23050 / 0 at tick 2111, clock 1:10.366 | 23050 / 0 at tick 2111, clock 1:10.366 |

and 55 frames into the countdown both fighters' positions are bit-identical to the console.
NOT yet identical: at tick 319 the pad fighter's x differs in the last bits (0x43E00E88 /
0x43E00E7F) and 166 of 2816 fighter words differ; not examined (candidates: effects and
camera fed by the game's random generators, which a replay does not reproduce, or a
remaining small arithmetic difference).

How the cause was found (ground-truth tests, port/tests/ps2float, run in PCSX2 2.7.303 with
`PCSX2.AppImage -nogui -fastboot -logfile <log> -- <elf>`; output arrives on the EE serial
port 0x1000F180 in the log):
1. test.c / check.py: every float operation class (FPU and vector unit: add, subtract,
   multiply, divide, square root, multiply-add, int <-> float) on 1500 inputs: the PC model
   agrees on all of them. So the model was right; the difference was above it.
2. libm_test.c / pack_libm.py / check_libm.py: a driver packed into one ELF with the game
   executable calls the game's ORIGINAL sinf, cosf, tanf, atanf, atan2f, asinf, acosf, sqrtf,
   powf, floorf. Against the port's newlib build: 3 to 59 of 1500 results one unit off.
3. Cause: THE PS2 COMPILER'S CONSTANTS. ee-gcc 2.96 converts decimal float constants by
   truncation (0.1f = 0x3DCCCCCC; newlib's S1 = 0xBE2AAAAA where the source comment says
   0xBE2AAAAB), doubles too, and its compile-time folding truncates (1.0f / 30.0f =
   0x3D088888, (int)(0.1f * 100.0f) = 9). A host compiler rounds to nearest. This affected
   every inexact float constant in the game's C, not only libm.
4. Fix: port/tools/eeconst.py rewrites preprocessed source: constants become hexadecimal
   constants with the truncated value, constant arithmetic the grammar exposes is folded
   with truncation, a double constant initialising a float object is truncated. undefined.py
   builds every software-float source through it (not the vector-library references, which
   were written for a host compiler). After it: libm 0 differences on all ten functions,
   and the table above.
Known gap: constant arithmetic that only appears after the compiler propagates variables is
folded by the host with rounding to nearest; not measured.

## Renderer, step 1: software reference of the GS (2026-10-05, night)

Decision (user): the renderer uses SDL3's GPU API; start with the 2D layer.

port/src/gs/gs_soft.c interprets what the unchanged game code sends: the frame's DMA source
chain on VIF1 (`Dma_Flush` -> D1_TADR, chain mode, tag transfer on) -> VIF1 commands (DIRECT
carries GIF data; a command word often sits in the DMA tag with its data in the next run) ->
GIF packets (PACKED / REGLIST / IMAGE) -> GS registers, texture uploads and primitives -> a
software rasteriser (sprites, triangles, strips, fans, lines; texture functions, alpha
blending, alpha and depth test, scissor, both contexts). It is the dependency-free reference
for tests and for the GPU renderer, which takes over at the "registers and primitives" stage.

GS memory is emulated with its real layout (4 MB; page / block / column order per pixel
format). This is required, not a nicety: the game uploads its 4-bit textures as 32-bit
images whose bytes are pre-arranged for the 4-bit layout (the loading screen: a 64 x 32
32-bit upload drawn as a 128 x 128 4-bit texture), and frame buffers are read back as
textures. The 8-bit and 4-bit column tables are generated from their first column.

Use: `BT3_GS=1 BT3_SHOT=<n>` writes port/build/shots/frame_NNNNN.ppm every n frames
(port/tools/ppm2png.py converts); `BT3_GS_DUMP=<frame>` lists that frame's primitives and
uploads; `BT3_GS_VERBOSE=1` prints per-frame counts. Compiled with hardware float (it is not
simulation code).

Verified by looking at the output (docs/port/img/): the loading screen (the sword-field
mini-game, character sprite and 30 swords, correct colours and transparency) and the game
logo overlay during the battle intro. The 3D scene is black: models go through the VU1
vertex programs, whose data the VIF1 interpreter currently skips. About 8 frames per second
(per-pixel C, no optimisation).

Not done: VU1 programs (stage, characters, most effects), bilinear filtering, fog,
mip-mapping, GS -> host transfers, the display registers (the shown buffer is guessed as the
one that received most pixels), GIF data on the GIF channel itself (`Dma_SendGif`, used for
some uploads: not yet routed to the interpreter).

SDL3: the 64-bit library is installed (3.4.12); the port is a 32-bit program and needs
`lib32-sdl3` (multilib repository), not installed yet.

## Renderer, step 2: SDL3 GPU back end for the same primitives (2026-10-05, night)

port/src/gs/gs_gpu.c (+ shaders/gs.vert, gs.frag, compiled with glslc and embedded by
undefined.py). gs_core.c keeps decoding and GS memory and hands every primitive, with the GS
registers current at that moment, to `GsGpu_Draw`; `GsGpu_FrameEnd` uploads the frame's
vertices and newly decoded textures in one copy pass, replays the draw list (one render pass
per run of draws to the same frame-buffer address), shows the buffer that received most
draws, and paces to 30 frames per second. `BT3_GS=gpu` selects it (`port/run.sh`).
- Render targets: one per GS frame-buffer address, 1024 x 1024 GS pixels at SCALE = 2, with a
  D32 depth buffer; GS z is scaled to 0..1 and compared GREATER / GREATER_OR_EQUAL as on the GS.
- Textures: decoded from GS memory to RGBA and cached by TEX0, TEXA and the upload generation
  of the pages they occupy. A frame buffer used as a texture is sampled directly, unless
  something was uploaded over it since it was drawn.
- Blending: ((A - B) * C >> 7) + D turned into source / destination factors (coefficients
  0, 1, C, -C, 1 - C; 1 + C is approximated by 1). Alpha is carried with 1.0 = 0x80.
- Texture function and alpha test are in the fragment shader.
- SDL GPU's clip space has +Y up on every back end: the vertex shader flips y.
Verified by reading the render target back (`BT3_SHOT=<n>` writes
port/build/shots/gpu_NNNNN.ppm): the loading screen and the logo overlay are drawn correctly
at twice the PS2's resolution, at full speed (docs/port/img/gpu_loading_screen.png).
The 32-bit SDL3 GPU device works on the development machine (Vulkan, NVIDIA; lib32-sdl3,
lib32-nvidia-utils).

Known gaps of the GPU back end: a buffer sampled while it is the draw target (skipped);
region clamp / repeat; destination alpha; frame-buffer masks; the texture cache keys on
upload generations, so a texture re-uploaded every frame (the loading screen does this for
every sprite) is decoded again each time (correct, wasteful: hash the contents instead);
no keyboard / pad input yet; the window shows nothing of the 3D scene (VU1 programs).

## Renderer, step 3: the vertex unit (VU1) as an interpreter (2026-10-05 / 06)

Decision: run the game's nine ORIGINAL vertex programs in an interpreter first
(port/src/gs/gs_vu1.c), instead of rewriting each as a shader straight away. Nothing was
known about what the programs do (the decompilation's notes say so), and an interpreter
gives the correct 3D scene for all nine at once and a reference to check shader versions
against later. Its output is GIF packets (XGKICK), so both back ends draw it unchanged.

- VIF1 side (gs_core.c `vif`): STCYCL, OFFSET, BASE, ITOP, STMOD, STMASK, STROW, STCOL, MPG,
  UNPACK (all formats, masks, modes, skipping and filling cycles, TOPS-relative addresses),
  MSCAL / MSCALF / MSCNT with the TOP / TOPS double-buffer flip.
- Interpreter: the instruction subset found by surveying the 1946 instruction pairs of the
  microcode block (upper: add / sub / madd / mul with broadcast, Q and I forms, the ACC
  forms, itof0, ftoi0, ftoi4, abs, clip; lower: lq, sq, lqi, sqi, sqd, ilwr, iswr, isw,
  integer add / and, branches, bal, jr, div, waitq, mr32, move, xtop, xgkick, fcand, fcor,
  fcget) plus neighbours; unknown instructions are counted and reported. Upper and lower
  halves of an instruction read their operands before either writes; Q has its latency;
  the clipping flags have their four-instruction delay. MAC / status flags are not modelled
  (no program reads them).
- A fact that cost a debugging round: BAL stores an instruction NUMBER and JR jumps to one
  (no division by 8).
- port/tools/vudis.py disassembles microcode (the used subset); a run that does not end
  dumps port/build/vu1_micro.bin and vu1_mem.bin.

How the game drives it (verified from the command stream of a battle frame): MPG uploads a
program at 0 (two commands, 399 instructions for the first battle program); UNPACK of 12
quadwords of constants at 0; BASE 141, OFFSET 436; MSCALF 0 (initialisation, ends at the
first E bit); then per batch UNPACK header (3 quadwords at TOPS) + vertices (3 quadwords each
at TOPS + 3), MSCNT. The program's main loop: transform by the matrix in vf1..vf4 and a
second one in vf5..vf8, divide, ftoi4, clip test with a branch to a clipper, XGKICK.

Verified by looking at the GPU read-back (docs/port/img/): both fighters are drawn
correctly (textured, cel-shaded, with their after-images) during the battle intro, and the
sky is drawn.
Wrong or missing: the sky shows as vertical stripes (first_stage_sky.png), the stage itself
is black in the frames looked at, the picture is slower than real time (about 15 frames per
second: the interpreter, thousands of small draws, and textures decoded again whenever
their upload generation changes).

## Renderer, step 4: the whole scene in the software reference (2026-10-06)

Verified by looking at it (docs/port/img/reference_full_scene.png): the software reference
draws a complete battle frame: sky, the plains stage, both fighters, the logo overlay.

What was missing, found by drawing only the first N vertices of a frame (`BT3_GS_STOP`):
the stage was drawn correctly and then wiped by the game's post-processing. Facts about how
the game uses the GS, all needed for a correct picture:
- GS memory map in battle: frame buffer at block 0 (fbp 0x000, 512 x 448, 32-bit), second
  buffer at fbp 0x070, DEPTH BUFFER at fbp 0x0E0 (ZBUF 0xE0, 24-bit: PSM 0x31), work buffers
  at fbp 0x150 (256 x 256), 0x170 (128 x 128), 0x1F6 / 0x1F8 (64 x 64): a down-sampling
  chain (glare / bloom), textures streamed to block 0x2A00 with palettes at 0x32C0.
- The stage is textured by STREAMING: each material's texture is uploaded to the same
  address (0x2A00) immediately before the triangles that use it, dozens of times per frame.
- The depth buffer is cleared by drawing sprites INTO it as a frame buffer (FRAME fbp 0xE0),
  and it is read as a texture (TEX0 tbp 0x1C00 with the depth formats 0x31 / 0x32 or as
  PSMT8H, i.e. its top byte as an 8-bit index). So depth must live in GS memory with the
  real layout: the depth formats are the colour formats with the block index XOR 0x18.
- FRAME.FBMSK (per-bit write mask) is used constantly: 0x00FFFFFF to write only the alpha
  byte, 0xFF000000 to protect it, and 0x3FFF with a 16-BIT view of the 32-bit frame buffer
  (512 x 896, drawn in 8-pixel strips with the texture shifted by 8 pixels: the "channel
  shuffle" that moves one half of every 32-bit pixel into the other).
  Ignoring the mask made that pass overwrite the whole picture.
- Depth clear between stage and fighters: 16 untextured strips, alpha 0, blending on (colour
  unchanged), depth test ALWAYS.
- DMA address registers: the game masks addresses to 28 bits (`& 0x0FFFFFFF`), which cuts PC
  stack addresses: `DMA_PHYS` in the decompilation keeps the pointer on PC. The GIF channel
  (`Dma_SendGif`, used for uploads outside the frame list) is routed to the front end too.

Two artefacts fixed afterwards (reference_full_scene.png shows the result):
- SHADOW drawn as a black shape near the horizon: the game switches PRMODECONT to 0 for its
  shadow passes, so the primitive attributes come from PRMODE (0x48: Gouraud, blended,
  untextured) and PRIM only gives the type. With that honoured, the fighter is first drawn
  from above into the 256 x 256 work buffer (fbp 0x150, VU1 program of 101 instructions) and
  the shadow then lies on the ground under it.
- DARK SPECKLES over the fighters: the black outline hull is drawn at almost the same depth
  as the body. The rasteriser's float weights summed to just under 1, so a flat triangle's
  depth came out one unit low on some pixels and the hull won. Weights are now double
  precision and depth / colour are rounded.
- The last pass of the frame draws the depth-buffer memory (cleared to 0 by then) as a
  24-bit texture over the picture with alpha 0x19: a uniform darkening (the intro's fade).
`BT3_GS_STOP=<n>` (with BT3_GS_FROM / BT3_GS_DUMP on the same frame) draws only the first n
primitives of that frame: the way these were located.

The GPU back end does not have depth-in-memory, depth as a texture, or FBMSK yet, so its
picture is behind the reference's (stripes, black stage). The post-processing chain reads
and writes single channels of the same memory in several formats; on the GPU those passes
need either the same tricks expressed with colour masks and format-converting shaders, or
native replacements of the effects (depth tint, glare, blur).

## Renderer, step 5: the GPU back end draws the whole scene (2026-10-06)

Decision (user): full-screen effects that depend on PS2 memory tricks get NATIVE versions;
everything else stays on the generic path. Step 1 of that, done:
- PRMODECONT / PRMODE: the GPU path reads the effective attributes (gGs.prim), so the shadow
  passes work as in the reference.
- FRAME.FBMSK in its whole-channel forms becomes the pipeline's colour write mask (alpha
  only, everything but alpha, ...). Partial masks cannot be expressed and are ignored.
- PS2-only passes are DROPPED in GsGpu_Draw (counted; `BT3_GS_VERBOSE` prints the number):
  drawing through a 16-bit view of the frame buffer; drawing into the depth buffer's memory;
  sampling the depth buffer's memory or a buffer's top byte as an 8-bit index; sprites that
  copy one frame buffer into another or into itself. About 240 primitives per battle frame.
  Triangles textured with a buffer are kept (the projected shadow).
Verified by reading the render target back (docs/port/img/gpu_full_scene.png): the same
frame as the reference's, at twice the resolution: sky, stage, both fighters with their
outlines, the shadow on the ground, the logo overlay.
Missing on purpose until their native versions exist, so the GPU picture is brighter and
sharper than the reference's: depth tint, glare, the intro's fade, any blur.
Speed is unchanged (about half real time): see the profile above, not addressed yet.

## Renderer fixes after the user looked at the window (2026-10-06)

- SMEARS TRAILING BEHIND CAMERA PANS ("a trail of textures"), stage picture stuck on the
  nearest geometry ever drawn: nothing cleared the depth buffer between frames. The game
  leaves the per-frame clear of colour and depth to Sony's library: `sceGsSetDefDBuff`
  builds a "drawing environment + clear sprite" GIF packet per buffer inside sceGsDBuff and
  `sceGsSwapDBuff` sends it every frame (Gfx_EndFrame step 8). Both were empty stand-ins.
  Implemented in port/src/gs/gs_sony.c from the documented layout.
- TREES WITHOUT LEAVES: the leaves are alpha-tested with a result exactly at the threshold
  (texture alpha 0x80 times vertex alpha 0x7F = 0x7F, test "alpha >= 0x7F"). The shader
  compared floats; it now rounds to the GS's integer first.
- TEXTURE CACHE: keyed on a content hash of the GS memory pages a texture and its palette
  occupy (the game streams stage textures to one address hundreds of times per frame; the
  old key changed on every upload). About 80 to 200 live textures instead of 2,048 churning,
  and the window now runs close to full speed.
- `BT3_GS_PROBE=x,y` (with BT3_GS_DUMP): the reference lists every primitive that reaches a
  pixel, with its state. This is how the leaves were diagnosed.
- CHARACTER OUTLINES ARE MISSING in the GPU picture, as expected after dropping the PS2-only
  passes: the outline is a full-screen effect (`ObjOutline_Draw`, src/sys/gfxm_a.c), not
  geometry. It reads a per-pixel object id that the models leave in the frame's alpha (kept
  in the depth page's spare top byte), finds where neighbouring pixels have different ids
  (two shifted, masked, subtracting copies), writes the edges into the frame's alpha and
  blends a dark rectangle (0x64 per channel) by it. First native effect to write.
  Design note for it: the render targets currently store alpha rescaled (1.0 = 0x80,
  clamped), which destroys ids above 0x80; the GS alpha byte needs its own exact channel
  (a second colour attachment written unblended under the alpha part of FBMSK).

## First native effect: the outline (2026-10-06)

Mechanism for native effects, used here for the first time:
- MARKER. The game builds the frame's display list first and the renderer runs it later, so
  the place of an effect travels in the list: `Port_GsMarker(effect)` (port/src/gs_marker.c)
  queues a one-register GIF packet writing the effect number to "register 0x7F", which the
  GS does not have. The game's effect function calls it under `#ifdef PORT`
  (`ObjOutline_Draw`: PORT_FX_OUTLINE = 1). gs_core.c passes it to `GsGpu_Native`; the
  software reference ignores it and keeps drawing the PS2 passes, which the GPU path drops.
- EXACT ALPHA BYTE. Every render target has a second colour attachment (R8, "aux") that
  receives the GS alpha byte unblended, under the alpha part of FBMSK; the colour texture
  keeps alpha rescaled for blending. Object numbers live in that byte.
- NATIVE PASS. In the replay, a native entry ends the current pass and runs its own pass on
  the colour texture alone (so the aux texture can be sampled), then normal drawing resumes.

The outline itself (shaders/outline.frag): the PS2 pass (verified reading of
`ObjOutline_Draw` / `ObjOutline_BuildClut`) maps the id byte through a table (id * 8, or
0x80 when that is 0; id 0xFF = no colour), subtracts the same image shifted by one pixel in
each axis so only positive differences remain, and blends a dark rectangle (0x64 per
channel) through the result. The native shader reads the id of the pixel and its four
neighbours one PS2 pixel away, applies the same table, and subtracts 0x64 / 255 where the
pixel's value is larger than a neighbour's. Where the ids come from (verified on a frame):
the frame's alpha is set to 0xFF, the fighters are drawn with their part numbers as alpha,
and that alpha is what the pass reads (on the PS2 through a copy in the depth page's top
byte). `BT3_FX_DEBUG=1` shows the numbers instead of the lines.
Verified by looking at the read-back (docs/port/img/gpu_full_scene.png): a dark line around
both fighters and between body parts with different table values.
Not matched exactly: the PS2 line's thickness and strength (it draws the edge image three
times, one line up, one down and centred, with different alphas); decoded textures keep
alpha rescaled, so an id passes through a rounding step (looked right, not proven exact).

## Frame time (2026-10-06)

Measured (`BT3_GS_VERBOSE` prints "time:" every 60 frames; `perf record` for the split), on
the development machine, replay01 in the window:
- The picture is CPU-bound on one thread; the graphics card is idle. In the fight a frame
  takes about 23 to 32 ms of work against a budget of 33.4 ms, so 30 frames per second is
  held most of the time and missed on a quarter to a third of the frames in the heavier
  stretches.
- Where it goes (after the fixes below): the VU1 interpreter about 45 % plus UNPACK 9 %,
  GS front end (GIF decoding, page hashes, GS memory writes for uploads) about 15 %, the GPU
  back end's recording 4 %, the game with its software float the rest.
Done: texture cache by content (was 30 %), last-texture shortcut, everything compiled -O2
and without PIC (the replay still reproduces the console's fight exactly), the upper and
lower NOPs of the vertex programs skipped, present mode MAILBOX (VSYNC added a wait for the
display on top of the game's own 30 Hz pacing). Pacing and timing now sit in gs_core.c.
Tried and switched off: a render thread (BT3_GS_THREAD=1). It is how the PS2 works (DMA
executes the list while the CPU runs the next frame), but here the list is still being
interpreted when the game changes data the list refers to; the user saw the camera jump.
It also gained little, because the renderer's own time (about 25 ms) is what sets the pace.
Next: the interpreter itself (pre-decoding the microcode when it is uploaded, whole-vector
operations for the full-mask cases, a straight copy for unmasked V4-32 UNPACK), and later
the shader versions of the programs, which remove this cost altogether.

## Vertex programs as shaders: program 0 (2026-10-06)

Decision (user): no interpreter tuning; go straight to shader versions. The interpreter
stays as the fallback for programs without a shader, as the reference to compare with
(`BT3_VU_INTERP=1` forces it), and for the software renderer.

Who costs what (measured, `BT3_GS_VERBOSE`; programs are told apart by uploaded size):
| uploaded size | program in the executable | used for | share of interpreted instructions |
|---|---|---|---|
| 399 | 4 (402) | stage and most static geometry | 27 to 39 % |
| 127 | 0 (128) | the fighters' models, two layers | 30 to 36 % |
| 101 | 3 (102) or 2 (100): not determined | the fighters again for the shadow buffer | 29 to 35 % |
| 451 | 6 (454) | a few runs per frame | under 1 % |

PROGRAM 0, read from its disassembly (port/tools/vudis.py) and confirmed on memory dumps
(`BT3_VU_DUMP=<frame>`):
- Constants, VU memory quadwords: 0..3 bone matrix A, 4..7 bone matrix B (columns), 8 / 9
  their pivots, 10..13 a matrix of which only the x components are set (the light direction:
  x' = light . v), 14..17 the matrix to GS screen coordinates, 18..21 the clip-test matrix,
  22 / 23 the two layers' colours (0..255 as floats). MSCALF 0 only precomputes 26..33
  (10..13 times A and B).
- A batch (one MSCNT): +0 a GIF tag "one A+D register", +1 TEX0_1 (texture layer), +2 TEX0_2
  (toon layer), +3 / +4 the primitive tags of the two layers (NLOOP = vertex count, PRIM in
  the tag: 0x5C and 0x25C, registers ST, RGBAQ, XYZF2), +5 the vertices: position with the
  blend weight in w, normal, (s, t, 1). One batch is ONE triangle strip.
- Per vertex: p = mix(B (pos - pivotB), A (pos - pivotA), weight); screen = C p, divided by
  w, to 12.4 fixed point with the integer depth in XYZF2; layer 0 gets (s, t) and colour 22;
  layer 1 gets u = 0.5 + 0.5 (light . normal'), v = 0 and colour 23: cel shading through a
  ramp texture. A triangle with a vertex outside the guard volume is dropped (ADC bit), there
  is no real clipping.
- Output: both layers in one kick, the second packet right behind the first.
Shader version: gs_vu1.c `hle_program0` sets each layer's TEX0 and PRIM through the normal
register path and passes the raw vertices and constants to `GsGpu_DrawVu0`;
shaders/vu0.vert does the skinning, outputs clip-space positions (the GPU clips and
interpolates with perspective; x, y, depth mapped as for GS vertices) and the two layers'
colour / coordinates; the fragment shader is the common one. Strips become triangle lists;
consecutive strips with the same state and constants are one draw call.
Verified: the same frame rendered with the interpreter and with the shader differs clearly in
587 of 917,504 pixels (0.06 %, edges); `work per frame` went from about 30 ms to about 24 ms
with a third of the interpreter's work gone.

## Next steps, in order

1. DONE: data (gen_data.py). 2. DONE except mathf.c / randf.c. 3. DONE (portsrc.py).
4. Platform layer for the 69 library functions: files from loose folders, memory, pad,
   stubs for sound and the GS for the headless build.
5. Link, boot to the battle loop, replay validation.

## 2026-10-05 (evening): fight camera inside a cliff, see-through fighter

- **Camera going through terrain (verified against console state `play07`, tick 666).** `StgCol_TraceSphere`
  handed `ColCapsule_GetLongSegDir` a 16-byte vector for a 32-byte segment. On PS2 the overflow lands in the next
  local (which the decomp had named `unused`); on PC the stack order differs and it overwrote the query's bounding
  box, so the camera's sweep never tested any terrain. Fixed in the decomp by declaring the scratch as two vectors
  (still matching). Lesson: a local that is "unused" next to an out-parameter is a hint of an undersized buffer;
  the camera of the pad fighter now equals the console's at that tick.
- **Stale work buffers.** A work buffer whose filling pass was dropped is no longer sampled (blocks of noise).
- **Vertex program 4 (stage) as a shader** (`vu4.vert`): strips go to the GPU unclipped, the program's own clipper
  is not needed.
- **See-through fighter ("alpha key", `GfxAlphaKey_Draw`)**: native pass (`alphakey.frag`, marker 2). Three things
  were needed: the pass itself; rectangles drawn into palette memory (the game paints alpha 0xF9 over the fighter
  palettes) must be executed in GS memory; texture alpha above 0x80 must survive (it was clamped at upload, now
  the shader rescales).
- Tools: `BT3_SHOT_FROM` / `BT3_SHOT_TO`, `port/tools/montage.py`, `BT3_CAMCHECK=<vblank>`.

## 2026-10-05 (night): VU1 listings, silhouette program as a shader

- The nine VU1 microprograms are documented assembly in the decomp now (`src/vu1/prog*.vsm`,
  `docs/systems/vu1/README.md`): 0 / 1 fighters, 2a / 2b flat-colour fighter (flat shadow / silhouette into the
  shadow page), 4 stage, 5 unused, 6 ground shadow, 7 debris, 8 animated stage objects.
- Measured per program: the interpreter's work in a fight frame was 92% program 2b (1,092 runs) and 7% program 6.
- Programs 2a / 2b go through program 0's shader (`hle_program2` in `gs_vu1.c`: the same batch, constants in
  other places, first layer only). Frame work about 21 ms -> about 13 ms.
  First version lost the shadows (my check used frames with both fighters in the air; the user noticed). Cause:
  the shadow camera's matrix is orthographic with a negative constant w (-862 measured); the PS2 only divides,
  a GPU clips it as behind the eye. The shader negates the whole position when w < 0. Also: the context comes
  from the PRIM in force (the shadow passes use PRMODE), and depth is clamped, not clipped. Checked on the first
  frames of the fight (fighters on the ground) against the interpreter: shadows equal by eye. Left in the interpreter: program 6 (6 runs per frame), 1, 7, 8.
- Not carried over from the originals: program 4's 0.5% enlargement of clipped triangles (see the decomp notes).
- **Program 6 (ground shadow) as a shader** (`vu6.vert`, `hle_program6`, `GsGpu_DrawVu6`): position through the
  screen matrix, s, t generated from the shadow camera matrix and the scale, flagged strip triangles skipped on
  the CPU, the texture coordinates rescaled for a frame buffer used as a texture. Checked on the first frames of
  the fight against the interpreter (shadows equal by eye). The interpreter now runs 0 times in replay01's first
  1,440 frames (48 s; the timing line is printed every 60 frames = 2 s, not every second as first written);
  frame work about 10 ms (mean of the 60-frame averages). Three single frames over the 33.4 ms
  budget remain (75.7 ms near frame 121, about 36 ms near frames 361 and 1261): not examined.
- **Slow single frames = pipeline creation.** `BT3_GS_VERBOSE` now prints a `slow:` line for every frame over
  budget (game / display list split, new textures and pipelines, time in each). Measured: the three slow frames
  were 46 ms, 21 ms, 21 ms of pipeline creation (10, 5, 2 pipelines), texture decoding only 2 to 9 ms. Fix: a
  pipeline is built from its key alone (`pipeline_create`), keys are appended to `bt3_pipelines.txt`
  (`BT3_PIPELINES` overrides the path; git-ignored) and all known ones are created at start (22 in 101 ms).
  Whole replay with the file present, two runs: no frame over budget, mean 10.8 ms, worst 29.1 ms. The first run
  on a machine (no file) still has the hitches; shipping a list with the game would remove that.

## 2026-10-05 (late): the menu overlay is part of the PC build

- `src/menu/*.c` (the DBZP.BIN overlay, 69 files) is compiled and linked with the rest: all files compiled
  unchanged. `port/tools/portsrc.py` lists them; `gen_data.py` also converts the overlay's one assembly data
  chunk and now assembles the data objects itself (that step was done by hand before and not scripted).
- `Progress_Main` exists twice (the real one and the replay / demo stand-in in `headless.c`): linked with
  `--wrap=Progress_Main`. `BT3_REPLAY` or `BT3_DEMO` take the stand-in; otherwise the real menus run
  (`port/run.sh menu`). The overlay load (`Overlay_Load`) reads nothing.
- Fixes needed to reach the title screen: `UNCACHED()` in movie.c under PORT; `StgVu_RotateZ / X / Y` (hand-written
  VU0 code in the stage module) added to the reference vector library. Movies are skipped (MPEG stand-ins).
- Verified by screenshots: memory card check, publisher logos, legal screen, title screen with "Press START".
- **Input** (`port/src/gs/gs_input.c`): keyboard and SDL gamepads into the game's pad buffer; mapping in the
  file's header. The user confirmed a controller works.
- **Pacing**: the vertical blank is the clock with a window (`Port_VBlank` waits on a 59.94 Hz grid): menus run
  at 60 frames per second, battles at 30, as the game asks. `BT3_UNCAPPED=1` removes the wait.
- `port/src/plat_crash.c`: a fatal signal writes a backtrace to the terminal and `bt3_crash.txt`.
- Open: the user reports the program closes when creating a save file (not reproduced yet). The memory card
  layer still answers "no card"; saves need the folder-backed card.
