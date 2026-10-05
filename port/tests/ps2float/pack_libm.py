#!/usr/bin/env python3
"""Packs the libm test driver and the user's game executable into one PS2 ELF: the game's two load segments at
their own addresses (so its routines can be called), the driver at 0x01000000 as the entry point, with $gp set
to the game's value. Usage: pack_libm.py <SLUS_216.78> <driver.bin> <out.elf>"""
import struct, sys
game = open(sys.argv[1], "rb").read()
drv = open(sys.argv[2], "rb").read()
phoff, = struct.unpack_from("<I", game, 0x1C)
phentsize, phnum = struct.unpack_from("<HH", game, 0x2A)
segs = []
for i in range(phnum):
    t, off, va, pa, fs, ms, fl, al = struct.unpack_from("<8I", game, phoff + i * phentsize)
    if t == 1 and ms:
        segs.append((va, game[off:off + fs], ms))
segs.append((0x01000000, drv, len(drv) + 0x10000))
hdr = bytearray(game[:0x34])
struct.pack_into("<I", hdr, 0x18, 0x01000000)          # entry
struct.pack_into("<I", hdr, 0x1C, 0x34)                # program headers right behind the ELF header
struct.pack_into("<I", hdr, 0x20, 0)                   # no section headers
struct.pack_into("<HHHHH", hdr, 0x2A, 0x20, len(segs), 0x28, 0, 0)
out = bytearray(hdr) + bytearray(0x20 * len(segs))
for i, (va, data, ms) in enumerate(segs):
    while len(out) % 0x1000:
        out.append(0)
    struct.pack_into("<8I", out, 0x34 + i * 0x20, 1, len(out), va, va, len(data), ms, 7, 0x1000)
    out += data
open(sys.argv[3], "wb").write(out)
