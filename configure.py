#!/usr/bin/env python3
"""Extracts the executables from the disc image, splits them with splat and writes build.ninja.

Usage: .venv/bin/python configure.py && ninja
"""

import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ISO = ROOT / "Dragon Ball Z - Budokai Tenkaichi 3 (USA) (En,Ja).iso"
DISC = ROOT / "disc"
BINUTILS = "tools/binutils/mips-ps2-decompals-"

# End of .sdata rounded up to the 128-byte section alignment the original link used.
ROM_PAD_TO = 0x2FF180

AS_FLAGS = "-EL -march=r5900 -mabi=eabi -G0 -no-pad-sections -Iinclude"
# Compiler output is assembled as o64 so that address arithmetic on 32-bit pointers expands to
# addiu/addu as Sony's assembler did; with eabi the modern gas emits daddiu/daddu.
CC_AS_FLAGS = "-EL -march=r5900 -mabi=o64 -no-pad-sections -mno-pdr -Iinclude"

# The game was built with Sony's ee-gcc 2.96 at -O2. Spike's code uses the default
# small-data threshold (-G8); the CRI middleware was built with -G0.
# -fno-strict-aliasing: every file matches with or without it, but with it the loading screen
# matches as plain C (the original reloads fields after stores), so it is taken as original.
CC = "tools/ee-gcc2.96/bin/ee-gcc"
CC_FLAGS = "-O2 -fno-strict-aliasing -Iinclude"
G_FLAGS = {"src/cri": "-G0", "src/menu": "-G0"}
G_DEFAULT = "-G8"

# Each target is one binary that is split, rebuilt and compared on its own.
# `subdir` is the target's folder under asm/ and src/; the main executable owns everything else.
TARGETS = [
    {
        "name": "SLUS_216.78",
        "yaml": "config/SLUS_216.78.yaml",
        "subdir": None,
        "original": "disc/SLUS_216.78.rom",
        "built": "build/SLUS_216.78.rom",
        "ld_scripts": ["build/SLUS_216.78.ld", "build/undefined_funcs_auto.txt",
                       "build/undefined_syms_auto.txt", "config/linker_script_extra.ld"],
    },
    {
        # Menu/UI code loaded at 0x334C00, right after the main executable's .bss.
        "name": "DBZP",
        "yaml": "config/DBZP.yaml",
        "subdir": "dbzp",
        "original": "disc/BIN/DBZP.BIN",
        "built": "build/DBZP.BIN",
        "ld_scripts": ["build/DBZP.ld", "build/dbzp_undefined_funcs_auto.txt",
                       "build/dbzp_undefined_syms_auto.txt"],
    },
]
SUBDIRS = {t["subdir"] for t in TARGETS if t["subdir"]}


def run(cmd, **kwargs):
    print("$", " ".join(shlex.quote(str(c)) for c in cmd))
    subprocess.run(cmd, check=True, cwd=ROOT, **kwargs)


def extract():
    if (DISC / "SLUS_216.78").exists() and (DISC / "BIN" / "DBZP.BIN").exists():
        return
    if not ISO.exists():
        sys.exit(f"missing disc image: {ISO.name}")
    run(["7z", "x", "-y", f"-o{DISC}", ISO, "SYSTEM.CNF", "SLUS_216.78", "BIN", "IRX"],
        stdout=subprocess.DEVNULL)


def make_rom():
    run([BINUTILS + "objcopy", "-O", "binary", "--gap-fill=0x00",
         f"--pad-to={ROM_PAD_TO:#x}", DISC / "SLUS_216.78", DISC / "SLUS_216.78.rom"])


def split(target):
    # splat shells out to mips-linux-gnu-* binutils; tools/bin aliases them.
    env = dict(os.environ, PATH=f"{ROOT / 'tools' / 'bin'}{os.pathsep}{os.environ['PATH']}")
    run([sys.executable, "-m", "splat", "split", ROOT / target["yaml"]], env=env,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def linked_c_files(yaml_path):
    """C files named by `c` subsegments in a splat yaml: the ones the linker script uses."""
    text = (ROOT / yaml_path).read_text()
    return {Path("src") / f"{name}.c" for name in re.findall(r"\[0x[0-9A-Fa-f]+, c, ([\w/]+)\]", text)}


def sources(top, suffix, subdir):
    """Files under top/ belonging to a target: its own subdir, or everything outside all subdirs."""
    found = []
    for path in sorted((ROOT / top).rglob(f"*{suffix}")):
        rel = path.relative_to(ROOT)
        # asm/**/nonmatchings holds per-function files pulled in by INCLUDE_ASM, not standalone units.
        if "nonmatchings" in rel.parts:
            continue
        owner = rel.parts[1] if rel.parts[1] in SUBDIRS else None
        if owner == subdir:
            found.append(rel)
    return found


def write_ninja():
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
        f"$as {CC_AS_FLAGS} $gflag include/gcc_prelude.inc $out.s -o $out && "
        f"{sys.executable} scripts/set_eabi64.py $out",
        "  description = CC $in",
        "",
        "rule ld",
        "  command = $ld -EL $scripts -Map $out.map -o $out",
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
    headers = " ".join(str(p.relative_to(ROOT)) for p in sorted((ROOT / "include").rglob("*.h")))
    for target in TARGETS:
        asm = sources("asm", ".s", target["subdir"])
        # Only compile C files that have a segment: work-in-progress files under src/ that are
        # not linked yet must not be able to break the build.
        linked = linked_c_files(target["yaml"])
        srcs = [p for p in sources("src", ".c", target["subdir"]) if p in linked]
        objs = [Path("build") / p.with_suffix(".o") for p in asm + srcs]
        elf = f"build/{target['name']}.elf"
        ok = f"build/{target['name']}.ok"

        for src, obj in zip(asm, objs):
            out.append(f"build {obj}: as {src} | include/macro.inc include/labels.inc")
        for src, obj in zip(srcs, objs[len(asm):]):
            gflag = next((g for d, g in G_FLAGS.items() if str(src).startswith(d + "/")), G_DEFAULT)
            out.append(f"build {obj}: cc {src} | {headers} include/gcc_prelude.inc")
            out.append(f"  gflag = {gflag}")
        out += [
            f"build {elf}: ld | {' '.join(map(str, objs))} {' '.join(target['ld_scripts'])}",
            f"  scripts = {' '.join('-T ' + s for s in target['ld_scripts'])}",
            f"build {target['built']}: rom {elf}",
            f"build {ok}: check {target['built']} {target['original']}",
            f"default {ok}",
            "",
        ]
    (ROOT / "build.ninja").write_text("\n".join(out))


def main():
    extract()
    make_rom()
    for target in TARGETS:
        split(target)
    write_ninja()


if __name__ == "__main__":
    main()
