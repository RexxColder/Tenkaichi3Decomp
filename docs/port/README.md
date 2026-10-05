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

## Next steps, in order

1. DONE: data (gen_data.py). 2. DONE except mathf.c / randf.c. 3. DONE (portsrc.py).
4. Platform layer for the 69 library functions: files from loose folders, memory, pad,
   stubs for sound and the GS for the headless build.
5. Link, boot to the battle loop, replay validation.
