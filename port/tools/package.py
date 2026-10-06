#!/usr/bin/env python3
"""Makes the release folder and zip: the finished port without any game data, plus the setup window.

    BT3_CC=clang64 python3 port/tools/undefined.py && BT3_CC=clang64 python3 port/tools/link.py
    port/setup/build.sh
    python3 port/tools/package.py          -> port/build/release/ and port/build/bt3-port-linux-x64.zip

Contents:  bt3 + bt3.dat   the game without the data tables of the original programs (strip_data.py)
           bt3-setup       the setup window: reads the user's own disc image and unpacks it into gamedata/
           lib/            SDL3, so that it does not have to be installed
           README.txt, licenses/
The user needs nothing but their disc image. Nothing from the disc is in the zip."""
import pathlib, shutil, subprocess, sys, zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "port/build/release"
ZIP = ROOT / "port/build/bt3-port-linux-x64.zip"

README = """Dragon Ball Z: Budokai Tenkaichi 3 - PC port (Linux, 64-bit)

1. Start  bt3-setup
2. Choose your own disc image of the USA release (SLUS-21678), an .iso file.
   The setup checks it and unpacks the game's data next to this file. About 4 GB are needed.
3. Press Play. Later, start  bt3-setup  again and press Play, or run  ./play.sh

In the game, F1 opens the settings: resolution, widescreen, controls and sound.
Saves are kept in saves/. A file placed in gamedata/mods/<same path as the original> replaces the original.

This package contains no game data. The game's data comes from your disc and stays on your computer.
The opening movie needs the program "ffmpeg" to be installed; everything else works without it.
"""

def main():
    exe = ROOT / "port/build/bt3_64"
    setup = ROOT / "bt3-setup"
    for need in (exe, setup, pathlib.Path(str(exe) + ".map")):
        if not need.exists():
            sys.exit(f"missing {need.relative_to(ROOT)}: build first (see the top of this file)")
    if OUT.exists():
        shutil.rmtree(OUT)
    r = subprocess.run([sys.executable, str(ROOT / "port/tools/strip_data.py"), "--exe", str(exe), "--out", str(OUT)], capture_output=True, text=True)
    print(r.stdout.strip())
    if r.returncode or "NOT matched" in r.stdout:
        sys.exit("strip_data.py failed or left game data in the program: not packaging\n" + r.stderr)
    shutil.copy2(setup, OUT / "bt3-setup")
    (OUT / "lib").mkdir()
    lib = next((p for d in ("/usr/lib", "/usr/lib64", "/usr/lib/x86_64-linux-gnu") for p in sorted(pathlib.Path(d).glob("libSDL3.so.0"))), None)
    if lib is None:
        sys.exit("libSDL3.so.0 not found")
    shutil.copy2(lib.resolve(), OUT / "lib/libSDL3.so.0")
    (OUT / "README.txt").write_text(README)
    (OUT / "play.sh").write_text('#!/bin/sh\n# Starts the game from its menus.\ncd "$(dirname "$0")" && BT3_GS=gpu exec ./bt3\n')
    (OUT / "play.sh").chmod(0o755)
    lic = OUT / "licenses"
    lic.mkdir()
    shutil.copy2(ROOT / "port/third_party/imgui/LICENSE.txt", lic / "dear-imgui.txt")
    shutil.copy2(ROOT / "port/third_party/newlib_libm/COPYING.NEWLIB", lic / "newlib.txt")
    (lic / "sdl3.txt").write_text("SDL3 (lib/libSDL3.so.0) is distributed under the zlib license: https://www.libsdl.org/license.php\n")
    for f in (OUT / "bt3", OUT / "bt3-setup"):
        subprocess.run(["strip", "--strip-debug", str(f)], check=False)  # the symbol names stay (crash reports use them)
    if ZIP.exists():
        ZIP.unlink()
    with zipfile.ZipFile(ZIP, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(OUT.rglob("*")):
            if p.is_file():
                info = zipfile.ZipInfo.from_file(p, "bt3-port/" + str(p.relative_to(OUT)))
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, p.read_bytes(), compresslevel=9)
    print(f"{ZIP.relative_to(ROOT)}: {ZIP.stat().st_size / 1e6:.1f} MB")
    for p in sorted(OUT.rglob("*")):
        if p.is_file():
            print(f"  {p.stat().st_size:>10,}  {p.relative_to(OUT)}")

main()
