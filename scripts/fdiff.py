#!/usr/bin/env python3
"""Compiles one C file and compares each of its functions against the original binary.

Usage: scripts/fdiff.py src/sys/heap.c [function ...]

Relocated fields (jump targets, %hi/%lo/%gp_rel immediates) are masked out, so a function can be
checked before the file is linked in. The linked build's byte comparison is the final word.
Run after `ninja`: target addresses come from the symbols in build/*.elf.
"""

import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BINUTILS = "tools/binutils/mips-ps2-decompals-"
CC = "tools/ee-gcc2.96/bin/ee-gcc"
ELFS = ["build/SLUS_216.78.elf", "build/DBZP.elf"]

# Bits of an instruction word that a relocation fills in.
RELOC_MASK = {
    "R_MIPS_26": 0x03FFFFFF,
    "R_MIPS_HI16": 0xFFFF,
    "R_MIPS_LO16": 0xFFFF,
    "R_MIPS_GPREL16": 0xFFFF,
    "R_MIPS_LITERAL": 0xFFFF,
}


def sh(*cmd):
    return subprocess.run(cmd, cwd=ROOT, check=True, capture_output=True, text=True).stdout


def gflag(src):
    return "-G0" if str(src).startswith("src/cri/") else "-G8"


def compile_c(src):
    out = ROOT / "build" / "fdiff" / Path(src).with_suffix(".o").name
    out.parent.mkdir(parents=True, exist_ok=True)
    asm = out.with_suffix(".s")
    g = gflag(src)
    r = subprocess.run([CC, "-O2", "-Iinclude", g, "-S", str(src), "-o", str(asm)],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stderr)
    if r.stderr:
        print(r.stderr, file=sys.stderr)
    sh(BINUTILS + "as", "-EL", "-march=r5900", "-mabi=o64", "-Iinclude", g, "-mno-pdr",
       "include/gcc_prelude.inc", str(asm), "-o", str(out))
    return out


def functions(obj):
    """name -> (offset, size) for functions defined in the object's .text."""
    funcs = {}
    for line in sh(BINUTILS + "readelf", "-sW", str(obj)).splitlines():
        p = line.split()
        if len(p) == 8 and p[3] == "FUNC" and p[6] != "UND":
            funcs[p[7]] = (int(p[1], 16), int(p[2]))
    return funcs


def reloc_masks(obj):
    """.text offset -> mask of relocated bits."""
    masks = {}
    in_text = False
    for line in sh(BINUTILS + "readelf", "-rW", str(obj)).splitlines():
        if line.startswith("Relocation section"):
            in_text = "'.rel.text'" in line
            continue
        p = line.split()
        if in_text and len(p) >= 3 and p[2] in RELOC_MASK:
            masks[int(p[0], 16)] = RELOC_MASK[p[2]]
    return masks


def text_bytes(obj):
    raw = (ROOT / "build" / "fdiff" / "text.bin")
    sh(BINUTILS + "objcopy", "-O", "binary", "-j", ".text", str(obj), str(raw))
    return raw.read_bytes()


def targets():
    """name -> (address, elf) for every function in the linked originals."""
    found = {}
    for elf in ELFS:
        if not (ROOT / elf).exists():
            continue
        for line in sh(BINUTILS + "nm", elf).splitlines():
            p = line.split()
            if len(p) == 3 and p[1] in "Tt":
                found.setdefault(p[2], (int(p[0], 16), elf))
    return found


def disasm(elf_or_obj, start, size, extra=()):
    out = sh(BINUTILS + "objdump", "-d", "-z", *extra, f"--start-address={start:#x}",
             f"--stop-address={start + size:#x}", str(elf_or_obj))
    lines = []
    for line in out.splitlines():
        if "\t" in line and ":" in line.split("\t")[0]:
            lines.append(" ".join(line.split("\t")[2:]))
    return lines


def original_words(elf, addr, size):
    raw = ROOT / "build" / "fdiff" / "orig.bin"
    sh(BINUTILS + "objcopy", "-O", "binary", str(elf), str(raw))
    base = int(sh(BINUTILS + "readelf", "-lW", elf).split("LOAD")[1].split()[1], 16)
    data = raw.read_bytes()[addr - base: addr - base + size]
    return struct.unpack(f"<{len(data) // 4}I", data)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    src = Path(sys.argv[1])
    wanted = sys.argv[2:]
    obj = compile_c(src)
    funcs = functions(obj)
    masks = reloc_masks(obj)
    text = text_bytes(obj)
    known = targets()

    ok = True
    for name, (off, size) in sorted(funcs.items(), key=lambda kv: kv[1]):
        if wanted and name not in wanted:
            continue
        if name not in known:
            print(f"{name}: no symbol with this name in the original; add it to the symbol files")
            ok = False
            continue
        addr, elf = known[name]
        mine = struct.unpack(f"<{size // 4}I", text[off:off + size])
        orig = original_words(elf, addr, size)
        bad = [i for i, (m, o) in enumerate(zip(mine, orig))
               if (m ^ o) & ~masks.get(off + i * 4, 0) & 0xFFFFFFFF]
        if not bad:
            print(f"{name}: OK ({size:#x} bytes)")
            continue
        ok = False
        print(f"{name}: {len(bad)} of {size // 4} instructions differ")
        a = disasm(elf, addr, size)
        b = disasm(obj, off, size, ("-r",) if False else ())
        for i in range(max(len(a), len(b))):
            left = a[i] if i < len(a) else ""
            right = b[i] if i < len(b) else ""
            mark = "!" if i in bad else " "
            print(f"  {mark} {i * 4:4x}  {left:<34} | {right}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
