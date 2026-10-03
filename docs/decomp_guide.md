# Decompiling a module (working notes)

The goal is C that compiles to the same bytes as the original with Sony ee-gcc 2.96 at `-O2`
(`-G8` for game code, `-G0` for `src/cri/`) with `-fno-strict-aliasing`. Every function gets a real name.

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

## -fno-strict-aliasing
The build uses it. The original reloads struct fields and global pointers after any store, which
is what this flag produces. Older files carry workarounds from before it was adopted (the
`(*&ptr->member)` form, anonymous unions around `count` or a whole struct); new code should be
written plainly, and the workarounds can be removed where the plain form still matches.

## Assembler prelude (include/gcc_prelude.inc)
Prepended to all compiler output so the modern gas encodes it as Sony's assembler did: `move`,
`break`, FPU hazard nops (compares, `mtc1`/`mfc1`, the `lui`/`mtc1` form of `li.s`), `cvt.w.s`,
`sqrt.s`, and which instruction in front of an unfilled branch moves into its delay slot.
Do not add per-file `__asm__` macro blocks; extend the prelude.
What Sony's assembler did with an unfilled `jal` / `j $31` / branch, and the prelude now does:
- The instruction in front moves into the delay slot, including a `li.s` that is a `.lit4` load
  (`li.s $f12,1.5707963 / jal f` becomes `jal f / lwc1 $f12,...`; a `return 0.28f;` leaf comes
  out as `jr $ra / lwc1 $f0,...` by the same mechanism, though the binary has no example yet).
- `mtc1`, `mfc1`, `ctc1`, `cfc1`, FPU compares and the `lui`/`mtc1` form of `li.s` (constants
  with a zero half, such as 21.0f) never move: the branch gets a nop.
- Nothing moves when the instruction before the candidate was a delay slot the compiler filled
  itself (`jal f / li $4,1 / li.s $f12,0.9 / jal g / nop`), provided the candidate is a single
  machine instruction; a macro that expands to several (`la $4,69132($17)`) still gives up its
  last one.
Known gaps, to check first if a function is off by a swapped or extra instruction next to a
branch: a load, store or `la` right after a compiler-filled delay slot and in front of an
unfilled branch is still moved (their size is not known to the macros); `break` and `sqrt.s`
are emitted as data and never move; `li.s` under `-G0` is untested; `sqrt.s` needs `$fN`
operands. The probes the prelude uses leave an unloaded `.gcc_prelude_scratch` section in every
object; the linker script discards it.

## Object boundaries and rodata alignment
Jump tables are 16-byte aligned inside an object's `.rodata`, so where the object *starts*
matters. If a file's rodata only lines up when the object begins earlier than the code you
decompiled, extend the C file backwards and pull the earlier functions in with `INCLUDE_ASM`
(splat moves each one's jump table into its generated .s file). `src/battle/btl_seq.c` is the
example: its code starts at 0x216AC0 but the object starts at 0x215540.
Things that only show when the file is linked (fdiff compares function by function and cannot
see them):
- A function-local table of a function that is still `INCLUDE_ASM` is not in its .s file (only
  jump tables are). Emit it with `INCLUDE_RODATA("asm/nonmatchings/<area>/<module>", D_XXXXXXXX);`
  next to the function; splat writes the .s for every symbol named that way. The same works for
  file-scope tables at the top of an object (`src/battle/btl_ai_cond.c`).
- The jump table in an `INCLUDE_ASM` .s file is only 8-byte aligned. If the object's rodata is
  not on a 16-byte boundary there, put `RODATA_ALIGN16();` in front of the `INCLUDE_ASM`.
- A non-static `inline` function is emitted at the END of the object by this compiler, whatever
  its place in the source. If a function is both called normally and inlined into a neighbour,
  write the body as a `static inline` helper and call it from both.

## Placeholder names across modules
Integrator only (agents working in parallel must NOT run it: it edits every file under `src/`
and `include/`, including other agents' work in progress). After adding names, run
`python3 scripts/apply_names.py`: it rewrites `func_XXXXXXXX` /
`D_XXXXXXXX` in `src/` and `include/` to the current names, so one module's rename does not
leave another module calling a symbol that no longer exists.

## More matching lessons
- A function whose early exits are all `return 0` and whose last statement is `return 1` keeps
  its separate success blocks; ending on `return 0` makes the compiler merge them.
- Whether a callee has already been compiled in the same file changes delay-slot filling and
  branch-likely choices in its callers. A mismatch of that kind is evidence of an original file
  boundary; as a stopgap, call through an aliased declaration
  (`extern T f2(void) __asm__("f");`).
- A helper the compiler can see is `const` (static, no side effects) lets callers keep values
  in registers across the call; try `static` on a small helper.
- Float literals: fdiff masks constant relocations, so compare each `.lit4` value with the
  original data. Decimal literals are truncated: use `3.14159265f`, `6.2831853f`,
  `1.41421356f`, `1.1666667f`.
- An object's `.lit4` and `.rodata` are each contiguous. If a function left in assembly owns
  constants in the middle of a file's pool, the file has to be split around it.

## Scratch files
Each agent uses its own subfolder for scratch scripts. Run Python scratch files with
`python3 file.py`, never as `./file.py` or `sh file.py` (a shell runs `import` as ImageMagick's
screenshot tool, which hangs). Never use `pkill` by name.
