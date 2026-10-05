#!/usr/bin/env python3
"""Turn the data the decompilation still keeps as assembly into host assembly for the PC build.

The matching build links some data sections straight from files that splat generates out of the user's own
SLUS_216.78 (asm/data/...). Those files keep pointers symbolic (`.word SomeFunc`), which is what a PC build needs:
the linker fills in the PC addresses. This tool rewrites them for the host assembler:
`.word` -> `.long` (a "word" is 2 bytes on x86), power-of-two `.align` -> `.p2align`, references to labels inside
assembly functions (jump tables of code the PC build does not use) -> 0, and each chunk starts at the same offset
modulo 16 as on the PS2, so data that must be 16-byte aligned stays aligned.

Usage: port/tools/gen_data.py [--asm DIR]     DIR = the decompilation's generated asm/ (default ../bt3/asm)
Output: port/build/gen/data/*.s  (generated from the game; never commit)"""
import argparse, pathlib, re

ROOT = pathlib.Path(__file__).resolve().parents[2]
KINDS = ("data", "sdata", "rodata", "lit4", "lit8", "bss", "sbss")
SECTION = {"data": ".data", "sdata": ".data", "rodata": ".data", "lit4": ".data", "lit8": ".data",
           "bss": ".bss", "sbss": ".bss"}  # everything writable: the original rodata is patched in places

def chunks(yaml):
    """(kind, name, PS2 address) of every data chunk the matching build takes from assembly."""
    out, base = [], 0x100000
    for l in yaml.read_text().splitlines():
        m = re.match(r"\s+- \[0x([0-9A-Fa-f]+), (\w+), ([\w/]+)\]", l)
        if m and m.group(2) in KINDS:
            out.append((m.group(2), m.group(3), int(m.group(1), 16) + base))
        m = re.match(r"\s+- \{ type: (\w+), vram: 0x([0-9A-Fa-f]+), name: ([\w/]+) \}", l)
        if m and m.group(1) in KINDS:
            out.append((m.group(1), m.group(3), int(m.group(2), 16)))
    return out

def convert(text, kind, addr):
    out = [f".section {SECTION[kind]}", ".p2align 4"]
    if addr & 15:
        out.append(f".space {addr & 15}")
    for l in text.splitlines():
        l = re.sub(r"/\*.*?\*/", "", l).rstrip()
        s = l.strip()
        if not s or s.startswith((".include", ".section", "nonmatching", "enddlabel")):
            continue
        m = re.match(r"dlabel (\S+)", s)
        if m:
            out += [f".globl {m.group(1)}", f"{m.group(1)}:"]
        elif s.startswith(".word"):
            arg = s[5:].strip()
            out.append("    .long 0" if arg.startswith(".L") else f"    .long {arg}")
        elif s.startswith(".align"):
            out.append(f"    .p2align {s[6:].strip()}")
        elif s.startswith((".byte", ".short", ".float", ".asciz", ".ascii", ".space", ".double")):
            out.append("    " + s)
        else:
            raise SystemExit(f"unhandled line: {s}")
    return "\n".join(out) + "\n"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--asm", default=str(ROOT.parent / "bt3/asm"))
    a = ap.parse_args()
    dst = ROOT / "port/build/gen/data"
    dst.mkdir(parents=True, exist_ok=True)
    for old in dst.glob("*.s"):
        old.unlink()
    n = 0
    for kind, name, addr in chunks(ROOT / "config/SLUS_216.78.yaml"):
        src = pathlib.Path(a.asm) / "data" / f"{name}.{kind}.s"
        if not src.exists():
            raise SystemExit(f"missing {src}: run the decompilation's configure.py first")
        (dst / f"{name.replace('/', '_')}.{kind}.s").write_text(convert(src.read_text(), kind, addr))
        n += 1
    print(f"{n} data chunks written to {dst.relative_to(ROOT)}")

main()
