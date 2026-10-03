# Decompiling a module (working notes)

The goal is C that compiles to the same bytes as the original with Sony ee-gcc 2.96 at `-O2`
(`-G8` for game code, `-G0` for `src/cri/`). Every function gets a real name.

## Loop
1. Read the functions: slice `asm/cod/*.s` (one huge file per range; never print it whole). Each
   function starts with `nonmatching NAME, 0xSIZE` / `glabel NAME` and ends with `endlabel NAME`.
2. Write `src/<area>/<module>.c` (+ a header in `include/<area>/`), `#include "common.h"` first.
   Types are in `include/types.h` (`s8 u8 s16 u16 s32 u32 s64 u64 f32`), `NULL` in `common.h`.
3. Record every new name in a symbol file under `config/symbols/` (format below).
4. Run `python3 scripts/fdiff.py src/<area>/<module>.c [function ...]`. It compiles the file and
   compares each function with the original, printing the differing instructions side by side
   (original on the left). `OK` means all non-relocated bits match.
5. Iterate until every function is `OK`.

`src/sys/heap.c` + `include/sys/heap.h` are a finished example.

## Symbol file format
    Name = 0x00123456; // type:func
    gGlobal = 0x002FF080; // size:0x8
Put a `//` comment line above each group saying what the function does / why the name fits.
If a name is a best guess, end the line with `// guess`. Names must be unique across all files in
`config/` (grep before adding). Functions: `Module_VerbNoun`. Globals: `gName`.

## Rules that matter for matching
- A C file must cover one contiguous address range, functions in address order, nothing skipped.
- Globals: declare `extern` with the right type and reference them by their symbol name
  (`D_XXXXXXXX` names from the disassembly work as-is; rename them in your symbol file when you
  know what they are). Do not define data in the C file yet.
- Small globals (size <= 8 bytes) are reached through `$gp` (`%gp_rel`); to reproduce that the
  `extern` must have a complete type of <= 8 bytes. An array like `extern s32 tbl[2];` qualifies;
  `extern s32 tbl[];` does not (it compiles to lui/addiu).
- A function that will not match after real effort: replace its body with
  `INCLUDE_ASM("asm/nonmatchings/<area>/<module>", FunctionName);` and keep your best C attempt
  directly above it inside `#if 0 ... #endif` with a one-line note on what differs.

## Things ee-gcc 2.96 is sensitive to
- Statement order of stores, and the order locals are declared/initialised (register allocation).
- `switch` vs `if/else` chains: a `beq 1 / slti <2 / beq 2 / beq 3` ladder was a `switch` with an
  explicit `case 0: break;`.
- `if (a != X && a != Y)` vs nested ifs produce different branch shapes; try both.
- A stray register copy (`move v1,s1`) usually means the original recomputed an expression the
  compiler had already seen, e.g. `(u8 *)free + free->size` instead of reusing a local.
- Loops: `for (i = 0; i < N; i++)` over a `$gp` array becomes a down-counter plus a walking
  pointer; write the plain `for` and let the compiler do it.
- Signed vs unsigned decides `slt`/`sltu`, `div`/`divu`, `lb`/`lbu`, `lh`/`lhu`.
- A `return x;` in the middle vs a result variable assigned and returned at the end changes
  branch-likely (`beql`/`bnel`) usage; try both shapes.
- Tail calls (`j func` after restoring `$ra`) come from `return f(...)` / a call as the last statement.
- `long` is 64-bit on this compiler: a `UL` constant in a multiply gives the
  `multu`/`mfhi`/`dsll32` widening sequence, and a `long` return value gives `dsll32`/`dsra32`.
- A bit mask tested with `1 << n` must be unsigned to get `sllv`/`and` (signed gives `srav`/`andi`).
- If the original reloads a global pointer after a store through it, access the member through
  `(*&ptr->member)` so the compiler cannot assume the pointer is unchanged.
- A list `count` that the compiler reorders around pointer stores matched only when declared in
  an anonymous union with a pointer (`union { s32 count; ListNode *countAlias; }`).
- A `jal` + branch where a tail call was expected means the function is non-void.
- Duplicated statements in both arms of an `if/else` are sometimes required; simplifying them
  collapses the branch.
- `buf = dst; return buf;` (copy the parameter, return the copy) can change which register a
  final test uses.
- `ei`/`di` and `sync.l` go in `__asm__ volatile("...")`.

## Linking a finished file
Add a `c` subsegment for its range in `config/SLUS_216.78.yaml` (and a `.rodata` subsegment if it
emits jump tables or strings), list any new symbol file in both yamls, then
`.venv/bin/python configure.py && ninja`. The build only counts if ninja itself succeeds.
The original padded each object's `.rodata` to 16 bytes and ours pads to 8: when a C file's
rodata does not end on a 16-byte boundary, start the following assembly rodata chunk 8 bytes early.
