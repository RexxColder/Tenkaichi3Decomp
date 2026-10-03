#!/usr/bin/env python3
"""Extracts the executable from the disc image, splits it with splat and writes build.ninja.

Usage: .venv/bin/python configure.py && ninja
"""

import os
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ISO = ROOT / "Dragon Ball Z - Budokai Tenkaichi 3 (USA) (En,Ja).iso"
DISC = ROOT / "disc"
BASENAME = "SLUS_216.78"
YAML = ROOT / "config" / f"{BASENAME}.yaml"
BINUTILS = "tools/binutils/mips-ps2-decompals-"

# End of .sdata rounded up to the 128-byte section alignment the original link used.
ROM_PAD_TO = 0x2FF180

AS_FLAGS = "-EL -march=r5900 -mabi=eabi -G0 -no-pad-sections -Iinclude"

# The game was built with Sony's ee-gcc 2.96 at -O2. Spike's code uses the default
# small-data threshold (-G8); the CRI middleware was built with -G0.
CC = "tools/ee-gcc2.96/bin/ee-gcc"
CC_FLAGS = "-O2 -Iinclude"
G_FLAGS = {"src/cri": "-G0"}
G_DEFAULT = "-G8"


def run(cmd, **kwargs):
    print("$", " ".join(shlex.quote(str(c)) for c in cmd))
    subprocess.run(cmd, check=True, cwd=ROOT, **kwargs)


def extract():
    if (DISC / BASENAME).exists():
        return
    if not ISO.exists():
        sys.exit(f"missing disc image: {ISO.name}")
    run(["7z", "x", "-y", f"-o{DISC}", ISO, "SYSTEM.CNF", BASENAME, "BIN", "IRX"],
        stdout=subprocess.DEVNULL)


def make_rom():
    run([BINUTILS + "objcopy", "-O", "binary", "--gap-fill=0x00",
         f"--pad-to={ROM_PAD_TO:#x}", DISC / BASENAME, DISC / f"{BASENAME}.rom"])


def split():
    # splat shells out to mips-linux-gnu-* binutils; tools/bin aliases them.
    env = dict(os.environ, PATH=f"{ROOT / 'tools' / 'bin'}{os.pathsep}{os.environ['PATH']}")
    run([sys.executable, "-m", "splat", "split", YAML], env=env, stdout=subprocess.DEVNULL)


def write_ninja():
    # asm/nonmatchings holds per-function files pulled in by INCLUDE_ASM, not standalone units.
    asm = sorted(p.relative_to(ROOT) for p in (ROOT / "asm").rglob("*.s")
                 if "nonmatchings" not in p.parts)
    srcs = sorted(p.relative_to(ROOT) for p in (ROOT / "src").rglob("*.c"))
    objs = [Path("build") / p.with_suffix(".o") for p in asm + srcs]
    elf = f"build/{BASENAME}.elf"
    rom = f"build/{BASENAME}.rom"
    ld_scripts = [f"build/{BASENAME}.ld", "build/undefined_funcs_auto.txt",
                  "build/undefined_syms_auto.txt", "config/linker_script_extra.ld"]

    out = [
        f"as = {BINUTILS}as",
        f"ld = {BINUTILS}ld",
        f"objcopy = {BINUTILS}objcopy",
        f"as_flags = {AS_FLAGS}",
        "",
        "rule as",
        "  command = $as $as_flags $in -o $out",
        "  description = AS $in",
        "",
        # ee-gcc's driver crashes when it runs its own assembler on a modern host, so compile
        # to assembly and assemble with the modern gas. gcc_prelude.inc makes `move` encode
        # as daddu, the way Sony's assembler did.
        "rule cc",
        f"  command = {CC} {CC_FLAGS} $gflag -S $in -o $out.s && "
        "$as $as_flags $gflag -mno-pdr include/gcc_prelude.inc $out.s -o $out",
        "  description = CC $in",
        "",
        "rule ld",
        f"  command = $ld -EL {' '.join('-T ' + s for s in ld_scripts)} -Map $out.map -o $out",
        "  description = LD $out",
        "",
        "rule rom",
        "  command = $objcopy -O binary --gap-fill=0x00 $in $out",
        "  description = OBJCOPY $out",
        "",
        "rule check",
        "  command = cmp $in && touch $out",
        "  description = CHECK $in",
        "",
    ]
    for src, obj in zip(asm, objs):
        out.append(f"build {obj}: as {src} | include/macro.inc include/labels.inc")
    headers = " ".join(str(p.relative_to(ROOT)) for p in sorted((ROOT / "include").rglob("*.h")))
    for src, obj in zip(srcs, objs[len(asm):]):
        gflag = next((g for d, g in G_FLAGS.items() if str(src).startswith(d + "/")), G_DEFAULT)
        out.append(f"build {obj}: cc {src} | {headers} include/gcc_prelude.inc")
        out.append(f"  gflag = {gflag}")
    out += [
        f"build {elf}: ld | {' '.join(map(str, objs))} {' '.join(ld_scripts)}",
        f"build {rom}: rom {elf}",
        f"build build/{BASENAME}.ok: check {rom} disc/{BASENAME}.rom",
        f"default build/{BASENAME}.ok",
        "",
    ]
    (ROOT / "build.ninja").write_text("\n".join(out))


def main():
    extract()
    make_rom()
    split()
    write_ninja()


if __name__ == "__main__":
    main()
