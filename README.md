# Tenkaichi3Decomp

A native PC port of **Dragon Ball Z: Budokai Tenkaichi 3** (PlayStation 2, USA release, SLUS-21678), built on
the matching decompilation in **BT3-Decompiled**. It is the game's own code compiled for PC, with a new
renderer, sound and input layer underneath; it is not an emulator.

This repository contains **no game data**. You need your own disc image of that release: the setup reads the
game's data from it, and it stays on your computer.

## What works

- The whole game loop: boot, opening movie, menus, saving, fights, split screen.
- A GPU renderer (Vulkan) with the PS2's effects (outline, glow, depth tint, distance blur), each switchable.
- Internal resolution from 1x to 8x; 4:3, 16:9, 21:9 and wider without stretching the fight.
- Music, voices and sound effects; keyboard and controllers, fully rebindable; two players.
- In the game, **F1** opens the settings.

Not there yet: online play (the goal of the project), and one drawing routine a small number of character
models need (`ObjSeam_TransformVtx`; the game stops if such a model is shown). See
[docs/port/README.md](docs/port/README.md) for the running log of what is done, what is verified and what is not.

## Requirements

- A 64-bit x86 processor and a graphics card with Vulkan support. Most of the work is on the processor (the
  game's arithmetic is reproduced exactly as on the PS2); a recent mid-range CPU is recommended.
- Linux (glibc 2.35 or newer) or Windows 10 / 11.
- About 4 GB of disc space, and your disc image as an `.iso` file.

## Installing a release

Unpack the release, start `bt3-setup` (`bt3-setup.exe` on Windows), choose your disc image, press Play. The
setup checks that the image is the unmodified USA release and unpacks the game's data next to the program.

## Building from source

```
python3 install.py --iso <your disc image>
```

does everything on Linux: checks the tools, reads the disc, builds the 64-bit program and runs the game's demo
fight as a test. `python3 install.py --check` lists what is missing. `port/setup/build.sh && ./bt3-setup` is
the same with a window.

The release archives are made with `port/release/build_linux.sh` (in a container, so that the result runs on
other machines) and, for Windows, `BT3_CC=win64` with a mingw-w64 cross-compiler; the details are in
[docs/port/README.md](docs/port/README.md).

## Layout

| Path | Contents |
|---|---|
| `src/`, `include/` | The game's code, from BT3-Decompiled; changes for PC are inside `#ifdef PORT` |
| `port/src/` | The PC side: memory, files, memory card, movies; `gs/` renderer, sound, input, settings window |
| `port/tools/` | The build: pointer rewriting for 64-bit, packaging, extraction |
| `port/setup/` | The setup window |
| `port/third_party/` | Dear ImGui (MIT), the maths library of newlib |
| `docs/` | Notes on the game (from the decompilation) and `docs/port/` on the port |

## Legal

This project is not affiliated with or endorsed by the game's developers, publishers or rights holders. It
exists for preservation, study and interoperability, and distributes none of the game's assets. All rights to
the game belong to their owners. You need your own legally obtained copy of the game to use it.
