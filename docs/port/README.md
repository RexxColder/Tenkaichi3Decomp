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

## Next steps, in order

1. DONE: data (gen_data.py). 2. DONE except mathf.c / randf.c. 3. DONE (portsrc.py).
4. Platform layer for the 69 library functions: files from loose folders, memory, pad,
   stubs for sound and the GS for the headless build.
5. Link, boot to the battle loop, replay validation.
