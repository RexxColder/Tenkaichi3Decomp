#!/usr/bin/env python3
"""Reports how much of the game's own code is decompiled to C, from the last linked build.

Counts bytes of .text that come from compiled C objects (build/src/**), minus functions those
files still pull in from assembly with INCLUDE_ASM. Library code (CRI, Sony SDK, libc, libgcc)
is excluded from the denominator; the ranges are from docs/text_map.md.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# (start, end) of Spike's code in each binary.
GAME_RANGES = {
    "SLUS_216.78": [(0x100258, 0x269228), (0x2BD230, 0x2BF6B0)],
    "DBZP": [(0x334C00, 0x3B0E04)],
}
SYMBOL_FILES = ["config/symbol_addrs.txt", "config/symbol_addrs_dbzp.txt"]


def in_ranges(addr, ranges):
    return any(lo <= addr < hi for lo, hi in ranges)


def c_text(map_path):
    """(address, size, object) of every .text contribution from a compiled C object."""
    out = []
    for m in re.finditer(r"^ \.text\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(build/src/\S+\.o)$",
                         map_path.read_text(), re.M):
        out.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3)))
    return out


def included_asm_bytes(obj):
    """Bytes of functions a C file still includes from asm/**/nonmatchings/."""
    rel = Path(obj).relative_to("build/src").with_suffix("")
    if rel.parts[0] == "dbzp":
        folder = ROOT / "asm" / "dbzp" / "nonmatchings" / Path(*rel.parts[1:])
    else:
        folder = ROOT / "asm" / "nonmatchings" / rel
    src = (ROOT / "src" / rel).with_suffix(".c").read_text()
    total = 0
    for name in re.findall(r"^INCLUDE_ASM\([^,]+,\s*(\w+)\);", src, re.M):
        m = re.search(r"^nonmatching \w+, 0x([0-9A-F]+)", (folder / f"{name}.s").read_text(), re.M)
        total += int(m.group(1), 16) if m else 0
    return total


def function_counts(name, ranges):
    """(named, total) game functions, from the split assembly and the C sources."""
    asm_dir = ROOT / "asm" / ("dbzp" if name == "DBZP" else "cod")
    labels = []
    for path in asm_dir.rglob("*.s") if name == "DBZP" else (ROOT / "asm").rglob("*.s"):
        if name != "DBZP" and "dbzp" in path.parts:
            continue
        for m in re.finditer(r"^glabel (\w+)\n\s+/\* [0-9A-F]+ ([0-9A-F]{8})", path.read_text(), re.M):
            labels.append((m.group(1), int(m.group(2), 16)))
    game = {n for n, a in labels if in_ranges(a, ranges)}
    return sum(1 for n in game if not n.startswith("func_")), len(game)


def main():
    for name, ranges in GAME_RANGES.items():
        map_path = ROOT / "build" / f"{name}.elf.map"
        if not map_path.exists():
            print(f"{name}: no build/{name}.elf.map, run ninja first")
            continue
        total = sum(hi - lo for lo, hi in ranges)
        done = 0
        for addr, size, obj in c_text(map_path):
            if in_ranges(addr, ranges):
                done += size - included_asm_bytes(obj)
        named, count = function_counts(name, ranges)
        print(f"{name}: {done:#x} / {total:#x} bytes of game code in C ({100 * done / total:.2f}%)")
        print(f"    functions still in assembly: {count} ({named} of them named)")


if __name__ == "__main__":
    main()
