# Rules for a decomp agent working alongside others

Read docs/decomp_guide.md first (workflow, tool, compiler and assembler quirks). Finished examples:
src/sys/heap.c, src/sys/file.c, src/sys/pad.c, src/battle/btl_pool.c and their headers.

- Run everything from the repo root. `python3 scripts/fdiff.py <file.c> [function ...]` is the
  ONLY build tool you run; it is safe in parallel. Do NOT run configure.py, splat, ninja or git,
  and do not edit config/*.yaml, configure.py, include/gcc_prelude.inc, existing sources/headers,
  other agents' symbol files, or anything under asm/ or build/.
- Write only the files your brief names: your C files, your headers, your one symbol file.
- Disassembly: asm/cod/*.s (main executable; find your range with
  `grep -ln "^glabel NAME$" asm/cod/*.s`) and asm/dbzp/000000.s (menu overlay, which calls into
  the main executable). The files are huge: slice with awk/grep/python, never print them whole.
    awk '/^glabel NAME$/{p=1} p; p&&/^endlabel/{exit}' asm/cod/FILE.s
  Data and strings: asm/data/cod/*.s (grep the D_ symbol). Callers: grep `jal +NAME`.
- Existing names with their evidence: config/symbols/*.txt and config/symbol_addrs.txt. Use the
  headers under include/ for modules already decompiled. Names must be unique across config/
  (grep before adding). fdiff looks functions up by NAME in the symbol files, so add a name to
  your symbol file before diffing a function you named.
- Every function ends with a descriptive name (`Module_VerbNoun`, globals `gName`) and a one-line
  comment in the C saying what it does. Structs go in the header with field offsets in comments;
  unknown fields are `unkXX`. Mark guessed names `// guess` in the symbol file, and say in the
  comment above each group what the evidence is.
- A C file covers one contiguous address range, in address order, nothing skipped. Several
  consecutive files with no gap are fine when the code is clearly several source files.
- A function that will not match after real effort: INCLUDE_ASM as the guide describes, with your
  best attempt above it in `#if 0` and a note on what differs. Do not let one hard function block
  the rest; do the small ones first.
- String literals, float constants (.lit4) and jump tables your C emits must be listed in the
  report with their original addresses: the integrator has to place them.
- Do not claim a match you did not see fdiff print `OK` for.
- If another agent's header changes under you, do not depend on it: declare a local view and say so.

## Report (final message)
Each file's exact address range (start, end exclusive); functions OK vs INCLUDE_ASM (name those,
say what differs); data structures identified; what the code does, separating what the matching
C verifies from what you infer; emitted strings/floats/jump tables with original addresses;
guessed names; existing names or comments you believe are wrong; anything the integrator must know.
