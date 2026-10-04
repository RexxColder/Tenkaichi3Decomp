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
An assembly rodata chunk is only as aligned as its contents: one that holds no jump table (a
plain table such as `gBtlStatCurve`) is placed right behind the previous object's data, so if
that ends 4 bytes short (a C file ending in a string) start the chunk 4 bytes early
(`[0x1EE7BC, rodata, cod/1EE7BC]` for data at 0x2EE7C0).
A link error `undefined reference to D_XXXXXXXX` / `Name` after linking a file means a name used
in C has no entry in a symbol file LISTED IN THE YAMLS (fdiff reads every file under
`config/symbols/`, the linker only the listed ones, and fdiff masks call targets, so an invented
or stale name still prints OK). Check the call target in the original and use the listed name.

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
- A load or store whose address is `symbol(reg)` or `symbol` (`lw $2,gTable($4)`, which expands
  to lui / addu / lw) gives up its last instruction to the branch ONLY when the assembler already
  knows the symbol is not small data, which in practice means the table is DEFINED earlier in the
  same file (a `const` table above the function, or an `INCLUDE_RODATA` at the top of the file).
  For an `extern` (the compiler writes its `.extern name, size` at the END of the output) the
  load stays in front of the branch, which gets a nop. Both assemblers agree on this, so it
  needs no prelude rule, but it is a test for where data was defined:
  `BtlCharSnd_GetBankMask` (`jr ra` / `lw` in the slot: `gBtlSndBankMask` is defined in
  `btl_char_flag_snd.c`), `AiThink_FindWeightColumn` (the column tables at the top of
  `btl_ai_cond.c`), against `BtlObj_Get` and `Pad_GetStatus` (`lw` / `jr ra` / `nop`: bss).
  If a function is off by exactly such a swapped pair, define the table in the file (or move the
  function into the file that has it) instead of touching the prelude.
`__gp_forget` uses `.set mips64` (it was `.set mips4`): under mips4 the assembler still kept the HI/LO hazard
nop after `mfhi` / `mflo` when an `mtc1` follows (`x % 360` converted to float, `EftBurst_Update`); under mips64
it does not. Changing it altered no other linked object (all compiler output was reassembled with both).
Known gaps, to check first if a function is off by a swapped or extra instruction next to a
branch: a single-instruction load, store or `la` (`lw $2,8($sp)`) right after a compiler-filled
delay slot and in front of an unfilled branch is still moved (their size is not measured by the
macros); `break` and `sqrt.s` are emitted as data and never move; `li.s` under `-G0` is untested; `sqrt.s` needs `$fN`
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
- A float constant of an `INCLUDE_ASM` function that lies in the MIDDLE of the file's `.lit4`
  (C functions before and after it have constants) is emitted in place with
  `LIT4_WORD(D_XXXXXXXX, 0x........);` in front of the `INCLUDE_ASM` (`BtlInput_Update` in
  `src/battle/btl_input.c`); the file need not be split. A constant at the start or end of the
  pool can simply stay in the neighbouring assembly `lit4` chunk.
- A non-static `inline` function is emitted at the END of the object by this compiler, whatever
  its place in the source. If a function is both called normally and inlined into a neighbour,
  write the body as a `static inline` helper and call it from both.

## Placeholder names across modules
Integrator only (agents working in parallel must NOT run it: it edits every file under `src/`
and `include/`, including other agents' work in progress). After adding names, run
`python3 scripts/apply_names.py`: it rewrites `func_XXXXXXXX` /
`D_XXXXXXXX` in `src/` and `include/` to the current names, so one module's rename does not
leave another module calling a symbol that no longer exists. It takes path substrings to skip
(files other agents are writing). Caution: it reads EVERY file under `config/symbols/`, also the
ones not yet listed in the yamls; applying those names to linked files breaks the link. While
unlisted symbol files exist, apply only the listed ones (list the file in both yamls first).
The skip arguments do not help with that: they name source files to leave alone, the symbol files are
still all read. Run a copy of the script that ignores the unlisted symbol files instead.
Renaming a function: replace the name on word boundaries over `config/`, `src/` and `include/`
together (aliases of the form `extern T f_(void) __asm__("Name");` carry the name in a string and must
follow), check first that the new name is not used anywhere, then reconfigure and rebuild.

## More matching lessons
- A function whose early exits are all `return 0` and whose last statement is `return 1` keeps
  its separate success blocks; ending on `return 0` makes the compiler merge them.
- Whether a callee has already been compiled in the same file changes delay-slot filling and
  branch-likely choices in its callers. A mismatch of that kind is evidence of an original file
  boundary; as a stopgap, call through an aliased declaration
  (`extern T f2(void) __asm__("f");`).
  It works in both directions, and decides merges: `BtlColl_Update` needed
  `BtlHit_CheckProximityAll` above it (appended to `btl_char_hit.c`), `BtlInput_TestAction` the
  readers of `btl_input.c` (appended there), `BtlMember_Damage` needed `BtlColl_NextPoolMember`
  (moved to the top of `btl_char_member.c`); while two callers of `BtlAi_ScaleByLevel` in
  `btl_ai_cond.c` match only with it OUTSIDE the file, so it became the last function of
  `btl_ai_seq.c`. When two files with different local views of the same structures are merged,
  keep each half's types and put cast macros between the halves
  (`#define gBtlAi ((AiThMgr *)gBtlAi)`, see the middle of `btl_ai_cond.c`); a cast through
  `*(T **)&global` changes register allocation, a plain cast does not.
  A whole-file merge can be done mechanically: append the second file (minus `#include "common.h"`),
  compile, and turn every `extern` of the second part that the compiler reports as `conflicting types`
  into `#define Name ((ret (*)(args))Name)` (a cast of the function's address; the call stays a direct
  `jal` and the code does not change). Point the `INCLUDE_ASM` folders at the merged file's stem,
  remove the second file's `c` / `.rodata` / `.lit4` lines from the yaml (the merged file's sections
  simply run on), re-run fdiff on the whole file. A deliberate non-libc `memset` prototype is reported as
  a conflict with the built-in: that one is a warning, leave the `extern` alone. A header that carries
  prototypes (not only types) cannot follow such macros: a third part needs `#undef`s first.
  An `INCLUDE_ASM` function that ends up in the middle of a merged file needs its constants emitted in
  place with `LIT4_WORD` (take the labels from its generated .s, the values from the original).
- A `beqz` that comes out as `beqzl` (or the reverse) with everything else equal is the usual symptom of
  a missing earlier definition in the file; five functions of the action-handler batch were fixed by
  merges alone (`BtlFx_UpdateGroundFx`, `BtlAct_AttackDashHandler`, `BtlAct_SuperRushDashHandler`,
  `BtlAct_SwitchArriveLand`, `BtlAct_KoSwitchFlyIn`). Try the merge with the neighbouring file before
  giving such a function up, even when no particular callee is suspected.
- A switch whose dispatch flips between `sll / lui / addu / lw table(reg) / jr` and
  `lui / sll / addiu / addu / lw 0(reg) / jr` when nothing in the function changed is decided by what
  the compiler has seen earlier in the file, not by the code (`BtlAct_SuperRushFollowHandler` in
  `btl_act_f.c`): adding or removing one declaration anywhere above it flips it. A redundant prototype
  directly in front of the function is the least intrusive fix; say so in a comment and re-check the
  function after every change to the file or its headers.
- A helper the compiler can see is `const` (static, no side effects) lets callers keep values
  in registers across the call; try `static` on a small helper.
- Float literals: fdiff masks constant relocations, so compare each `.lit4` value with the
  original data. Decimal literals are truncated: use `3.14159265f`, `6.2831853f`,
  `1.41421356f`, `1.1666667f`.
- An object's `.lit4` and `.rodata` are each contiguous. If a function left in assembly owns
  constants in the middle of a file's pool, the file has to be split around it.
- More merge mechanics (effect batch). When the second part's HEADER (not a local `extern`) declares a name the
  first part already declared with another type, hide the header's declaration and cast afterwards:
  `#define gEftBurst gEftBurst_eDecl` / `#include "battle/eft_e.h"` / `#undef gEftBurst` /
  `#define gEftBurst ((EftTransWork *)gEftBurst)`. A plain cast also works on the left of an assignment (this
  compiler accepts a cast as an lvalue); a global that is a structure takes `(*(T *)&name)`; a structure tag
  both parts define takes `#define EftView EftViewE` at the top of the second part. A function that the second
  part DEFINES and the first part declared with other types cannot be a cast macro (it would rewrite the
  definition): give the first part an aliased declaration,
  `extern void f_e(A *) __asm__("f");` / `#define f f_e`, and `#undef f` at the top of the second part
  (`src/battle/eft_e.c`, `src/battle/eft_d_b.c`). A file can also be cut in two and its head appended to the
  previous file; both pieces then carry a copy of the preamble.
- Placing a new file's data without trusting notes: link the `c` subsegment first, build with `ninja -k 0`, and
  search each object's `.rodata` / `.lit4` / `.sdata` bytes in the original image (relocated words masked) in
  address order. Every file of the effect batch was placed that way; the gaps between the hits are the constants
  and tables of INCLUDE_ASM functions and stay assembly chunks. Do it again after the `.rodata` subsegments are
  listed: only then does splat write the jump tables into the INCLUDE_ASM .s files and the `INCLUDE_RODATA` .s
  files at all (before that the file does not assemble, or its `.rodata` is too small).
- VU0 macro code cannot be INCLUDE_ASM: in per-function files splat writes the accumulator operand as `ACC`
  (in the big chunks as `$ACC`), which the assembler rejects. Cut the C file around such functions and leave
  them an `asm` chunk (`stg_a.c` / `cod/140C68` / `stg_a_b.c`).
- A C file may call a function by a name that exists only in a symbol file that is not listed yet (another
  agent's): it links only by the placeholder. Check each undefined reference against the `jal` target in the
  original, never by the name's look.
- The private copy of `apply_names.py` should select the symbol files by "listed in the yaml", and skip the
  SOURCE files of agents that are still working by stem, not by prefix (stems of different waves share prefixes).
- A caller that needs a callee DEFINED above it, when the callee itself is still INCLUDE_ASM (second effects
  wave): compile the callee's C attempt, but let the assembler skip its output.
  `ASM_STUB_BEGIN();` / the attempt / `ASM_STUB_END();` / `INCLUDE_ASM(...)` (macros in include/include_asm.h;
  they emit `.if 0` and `.endif` as top-level assembly, which this compiler writes out in source order around
  the function). The compiler has then seen a definition, the code still comes from the original. It made
  `EftRibbon_Update`, `EftRibbon_Draw`, `EftRibbon_SetEnd` (callees `EftRibbon_PlaceStrip`, `EftRibbon_DrawStrip`,
  `EftRibbon_DrawKind1`) and `EftAura_ChangeType` (callee `EftAura_SetType`) match. Use the real attempt, not an
  empty body: the compiler works out for itself that a function without side effects is `const` and then treats
  its callers differently. Everything the attempt emits inside its function (jump tables, the constants of
  local initialisers, `li.s` literals) is skipped with it; a function-local `static` would not be. It only
  helps callers LATER in the file: wrapping every attempt of the wave this way changed no other function.
- fdiff masks `$gp`-relative offsets as it masks every relocation, so two small globals stored in the wrong
  ORDER print OK and only show when the file is linked (`EftAuraMgr_Init`, `EftGlowMgr_Init`: two pointers
  set to the same value). In `a = b = x;` the store to `a` is emitted FIRST: put the global the original
  stores first on the left.
- A whole-file merge changes the alignment base of the second part's read-only data: jump tables are aligned
  to 16 bytes relative to the START of the object's `.rodata`. If the first part's data starts 8 bytes off a
  16-byte boundary, a table of the second part that the original had on a boundary moves by 8 (eft_y + eft_z:
  0x2ED310 became 0x2ED308). That is evidence that the two are not one object beginning at the first part's
  data; cut the first file where its read-only data ends and merge only the tail (eft_y.c was cut at 0x195038).
- Tools from the second effects integration (build/scratch_integrate5/): `merge5.py` does the mechanical merge
  including conflicts that come from a HEADER the second part includes (it hides the header's declaration with
  `#define N N__p2` around the `#include` and adds the cast macro built from the header's own text);
  `layout.py` + `spec.py` generate the `.rodata` / `.lit4` / `.sdata` subsegments from each object's measured
  section size, so only the START address of each file's data has to be found (`place.py`), and the gaps become
  assembly chunks by themselves. A gap that is only alignment padding in front of a C file's section needs no
  chunk (the next object's own alignment produces it).
- After a re-split that creates or changes the .s files of INCLUDE_ASM / INCLUDE_RODATA (a `.rodata` subsegment
  added or resized), ninja does not rebuild the C object that includes them: delete the object. The symptom
  is an undefined `jtbl_XXXXXXXX` at link, or a section of the old size.
- A global that only unlinked files use may have no entry in any symbol file although every agent "knows" its
  name (`gEftZapMgr`): fdiff does not need the address of a `$gp` global, the linker does. Find it from the
  `$gp` offset in the original (`_gp` = start of `.lit4` + 0x7FF0) and add it to the defining module's file.

- Lessons of the last integration (eft_ae, col_b / col_c, bobj, the AI sequence; build/scratch_integrate6/):
  - Mechanical merge when the SECOND part defines functions that the first part declared with other types and the
    second part's header also declares them: `merge5.py` then hides the header's declarations and adds cast
    macros, which rewrite the definitions (parse errors at each definition). Give it the aliased declarations
    instead: `WORKDIR/sub1.txt` replaces each such `extern` of part 1 by
    `extern T f_a(args) __asm__("f");` / `#define f f_a`, and `WORKDIR/pre2.txt` puts the `#undef f` lines (and
    the `#undef` of any macro constant both headers define, `BOBJ_NODE_MAX`) at the top of part 2
    (`src/battle/bobj_a.c`). The copy of merge5.py in scratch_integrate6 accepts `\n` in the replacement text.
  - Replacing a block of INCLUDE_ASM lines at the top of a linked file with their C (btl_ai_seq_a.c into
    btl_ai_seq.c) moves only the START of the file's `.lit4`: the assembly chunk in front of it was those
    functions' constants. List the file's `.lit4` at the chunk's start and delete the old line; `.rodata` does
    not move (the jump tables were already in the object through the INCLUDE_ASM .s files), and an
    `INCLUDE_RODATA` table becomes the local initialiser of the function that owns it.
  - An empty function (a stripped assert) that callers in ANOTHER file need to be known as side-effect free:
    declare it `__attribute__((const))` in the callers' file, and give the defining file the same prototype in
    front of the definition (`EftTexSet_CheckCount`, eft_ae.c / eft_det_a.c). It replaces a merge.
  - `place.py` cannot place a `.rodata` that is mostly jump tables (every word is a relocation, so it "matches"
    everywhere): take the address from the `jtbl_` labels of the assembly chunk that the file's code range owns,
    link, and check the object's section size against the span. For a file with an `INCLUDE_RODATA` the `.rodata`
    subsegment has to be listed (with a provisional end) before the object can be assembled at all.
  - `enable.py` / `tryall.py` / `stub.py` expect `#if 0` alone on its line; an attempt written as
    `#if 0 /* comment` ... `#else` INCLUDE_ASM `#endif` (eft_ae.c) has to be tried by hand.
  - With every symbol file listed, `scripts/apply_names.py` runs as is. Order that worked: list the new symbol
    files in both yamls, check duplicates over ALL symbol files, run the name pass, pass the gate, and only then
    add the new `c` subsegments. All five new objects then linked byte-identical at the first attempt.

## Scratch files
Each agent uses its own subfolder for scratch scripts. Run Python scratch files with
`python3 file.py`, never as `./file.py` or `sh file.py` (a shell runs `import` as ImageMagick's
screenshot tool, which hangs). Never use `pkill` by name.
