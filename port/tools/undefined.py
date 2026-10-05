#!/usr/bin/env python3
"""Compile the game sources to 32-bit host objects and list what a link would still need, by kind.
Usage: port/tools/undefined.py [-v]   (objects go to port/build/obj)"""
import collections, concurrent.futures, pathlib, re, subprocess, sys
import portsrc
import eeconst

ROOT = pathlib.Path(__file__).resolve().parents[2]
OBJ = ROOT / "port/build/obj"
CC = ["gcc", "-m32", "-std=gnu89", "-c", "-O1", "-g", "-fno-strict-aliasing", "-ffp-contract=off", "-fcommon", "-w", "-fno-pic", "-fno-stack-protector", "-fno-builtin", "-msoft-float", "-mno-sse", "-mno-mmx", "-malign-double", "-include", "port_libm.h",
      "-Iinclude", "-Iport/include", "-include", "port_compat.h"]
TEXT_END, GAME_END = 0x2BF6B0, 0x273CF0  # end of all code; end of game code (libraries follow)

def compile_ee(cmd, src, o):
    """Compiles src with the PS2 compiler's float constants: preprocess, eeconst.transform, compile the result."""
    pre = [a for a in cmd if a != "-c"] + ["-E", str(src)]
    r = subprocess.run(pre, cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        return r
    i = pathlib.Path(str(o)[:-2] + ".i")
    i.write_text(eeconst.transform(r.stdout))
    return subprocess.run(cmd + ["-fpreprocessed", str(i), "-o", str(o)], cwd=ROOT, capture_output=True, text=True)

def cc(f):
    o = OBJ / (str(f.relative_to(ROOT / "src")).replace("/", "_")[:-2] + ".o")
    src, done, missing = portsrc.prepare(f)
    r = compile_ee(CC + [f"-I{f.parent}"], src, o)
    if missing:
        print("no C for:", f.name, " ".join(missing))
    return o if r.returncode == 0 else None, f, r.stderr

def main():
    OBJ.mkdir(parents=True, exist_ok=True)
    fs = portsrc.sources()
    with concurrent.futures.ProcessPoolExecutor(16) as ex:
        res = list(ex.map(cc, fs))
    bad = [(f, e) for o, f, e in res if o is None]
    for f, e in bad:
        print("FAILED", f.name, next((l for l in e.splitlines() if "rror" in l), "")[:150])
    objs = [str(o) for o, _, _ in res if o]
    objs += [str(o) for o in sorted((ROOT / "port/build/obj_data").glob("*.o"))]  # from gen_data.py
    # PC-only code: the vector-library references under the game's names, and port/src.
    for f in sorted((ROOT / "port/third_party/newlib_libm").glob("*.c")):  # the PS2's maths library, software float
        o = OBJ / ("nl_" + f.stem + ".o")
        r = compile_ee(["gcc", "-m32", "-std=gnu89", "-c", "-O1", "-g", "-msoft-float", "-mno-sse", "-mno-mmx",
                        "-malign-double", "-fno-strict-aliasing", "-ffp-contract=off", "-fno-builtin", "-w",
                        "-Iport/include", f"-I{f.parent}", "-include", "newlib_shim.h"], f, o)
        if r.returncode:
            print("FAILED", f.name, r.stderr.splitlines()[0][:150])
        else:
            objs.append(str(o))
    for f in sorted((ROOT / "port/src/gs").glob("*.c")):  # renderer: ordinary host code, hardware float
        o = OBJ / ("gs_" + f.stem + ".o")
        r = subprocess.run(["gcc", "-m32", "-std=gnu99", "-c", "-O2", "-g", "-msse2", "-mfpmath=sse", "-fno-strict-aliasing",
                            "-Wall", "-Wno-unused", "-Wno-misleading-indentation", str(f), "-o", str(o)], cwd=ROOT,
                           capture_output=True, text=True)
        if r.returncode:
            print("FAILED", f.name, "\n".join(l for l in r.stderr.splitlines() if "error" in l)[:400])
        else:
            objs.append(str(o))
    for f in [ROOT / "src/port/vu0_a.c", ROOT / "src/port/vu0_b.c"] + sorted((ROOT / "port/src").glob("*.c")):
        o = OBJ / ("pc_" + f.stem + ".o")
        names = ["-include", "vu0_names.h", "-DREF_VU0_EXTERN_ARITH"] if f.parent.name == "port" and f.parent.parent.name == "src" else []
        soft = [] if f.name == "plat_libm.c" else ["-msoft-float", "-mno-sse", "-mno-mmx", "-include", "math.h", "-include", "stdlib.h", "-include", "port_libm.h"]
        cmd = ["gcc", "-m32", "-std=gnu99", "-c", "-O1", "-g", "-malign-double", "-fno-strict-aliasing",
               "-ffp-contract=off", "-fno-builtin", "-w", "-Iinclude", "-Iport/include", "-Iport/src"] + soft + names
        if soft and not names:  # the port's own soft-float code: PS2 constants; the vector references are written for the host
            r = compile_ee(cmd, f, o)
        else:
            r = subprocess.run(cmd + [str(f), "-o", str(o)], cwd=ROOT, capture_output=True, text=True)
        if r.returncode:
            print("FAILED", f.name, r.stderr.splitlines()[0][:150])
        else:
            objs.append(str(o))
    defined, undef = set(), collections.Counter()
    out = subprocess.run(["nm", "-A"] + objs, capture_output=True, text=True).stdout
    for l in out.splitlines():
        m = re.match(r"\S+:\s*([0-9a-f]*)\s+(\w)\s+(\S+)$", l)
        if not m:
            continue
        if m.group(2) == "U":
            undef[m.group(3)] += 1
        elif m.group(2) not in "tdbr":
            defined.add(m.group(3))
    addr = {}
    for p in (ROOT / "config/symbols").glob("*.txt"):
        if p.name.startswith("menu"):
            continue
        for l in p.read_text().splitlines():
            m = re.match(r"(\w+)\s*=\s*0x([0-9A-Fa-f]+)", l)
            if m:
                addr[m.group(1)] = int(m.group(2), 16)
    kinds = collections.defaultdict(list)
    for s in undef:
        if s in defined:
            continue
        a = addr.get(s)
        if a is None:
            m = re.match(r"(?:func|D|jtbl)_([0-9A-F]{8})$", s)
            a = int(m.group(1), 16) if m else None
        k = ("no address (host library or compiler helper)" if a is None else "game function" if a < GAME_END
             else "library function (SDK / CRI / libc)" if a < TEXT_END else "data")
        kinds[k].append(s)
    print(f"{len(objs)} objects, {len(defined)} global symbols defined")
    for k, v in sorted(kinds.items()):
        print(f"{len(v):5d}  {k}")
        if "-v" in sys.argv:
            print("       " + " ".join(sorted(v)))
    (ROOT / "port/build/undefined.txt").write_text(
        "".join(f"{k}\t{s}\n" for k, v in sorted(kinds.items()) for s in sorted(v)))

if __name__ == "__main__":
    main()
