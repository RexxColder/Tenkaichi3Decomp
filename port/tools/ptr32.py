#!/usr/bin/env python3
"""The PS2 memory model in a 64-bit build: makes every pointer of the GAME's code 4 bytes wide.

Works on preprocessed C (the .i files of undefined.py). Text that comes from the game's own files (src/, include/)
is rewritten; text from the port's files and from system headers is left alone, so the port's code keeps ordinary
64-bit pointers while every structure, global and local of the game keeps its PS2 size and layout.

  1. every `*` of a declarator or type name      ->  `* __ptr32 __uptr`      (clang, -fms-extensions)
  2. a call through a function pointer  X(args)  ->  ((__typeof__(*(X)) *)(X))(args)
        (clang's code generator cannot call through a 4-byte function pointer; the cast makes an ordinary one)
  3. a pointer passed to a variadic function     ->  ((void * __ptr64)(arg))
        (a 4-byte pointer in a variable argument list would be read as 8 bytes by the C library)

The parse is done with libclang on the unchanged text; the edits are applied by byte offset afterwards.
All game memory has to lie below 4 GB for this to hold (see docs/port/README.md).
"""
import bisect, ctypes, re, sys
import clang.cindex as ci
from clang.cindex import CursorKind as K, TokenKind, TypeKind

P32 = "* __ptr32 __uptr "
TYPE_EXPRS = {K.CSTYLE_CAST_EXPR, K.CXX_UNARY_EXPR, K.COMPOUND_LITERAL_EXPR, K.DECL_STMT}  # expressions that contain a type name

def is_type_star(kind):
    """Whether a `*` token annotated with this cursor kind belongs to a declarator or type name. None = not sure."""
    if kind in TYPE_EXPRS or kind.is_declaration() or kind.is_reference():
        return True
    if kind == K.UNEXPOSED_EXPR:
        return None
    if kind.is_expression() or kind.is_statement():
        return False
    return None

def origins(data):
    """[(byte offset, is game text)] from the line markers of the preprocessed text."""
    out = [(0, False)]
    for m in re.finditer(rb'^# \d+ "([^"\n]*)"[^\n]*\n', data, re.M):
        f = m.group(1)
        # the game's sources are compiled from prepared copies under port/build/gen/src; its headers are in include/
        game = b"port/build/gen/src/" in f or (not f.startswith(b"/usr") and not f.startswith(b"<") and b"/port/" not in f and
                                               not f.startswith(b"port/"))
        out.append((m.end(), game))
    return out

def apply(data, edits):
    edits.sort(key=lambda x: (x[0], x[3]))
    out, pos = [], 0
    for off, length, ins, _ in edits:
        out.append(data[pos:off])
        out.append(ins.encode())
        pos = off + length
    out.append(data[pos:])
    return b"".join(out)

def parse(data, args):
    tu = ci.Index.create().parse("in.i", args=args, unsaved_files=[("in.i", data.decode("latin-1"))])
    errs = [d for d in tu.diagnostics if d.severity >= 3]
    if errs:
        raise RuntimeError(f"{errs[0].spelling} (line {errs[0].location.line})")
    return tu

def game_test(data):
    org = origins(data)
    starts = [o for o, _ in org]
    return lambda off: org[bisect.bisect_right(starts, off) - 1][1]

def transform(path, args, stats=None):
    """Returns the rewritten text of the preprocessed file `path` (parsed with the clang arguments `args`)."""
    data = open(path, "rb").read()
    # pass 1: the stars of declarators and type names
    game = game_test(data)
    tu = parse(data, args)
    edits = []  # (offset, length removed, inserted text, order)
    toks = list(tu.get_tokens(extent=tu.cursor.extent))
    n = len(toks)
    tarr = (ci.Token * n)(*toks)
    carr = (ci.Cursor * n)()
    ci.conf.lib.clang_annotateTokens(tu, tarr, n, carr)
    for i, t in enumerate(toks):
        if t.kind != TokenKind.PUNCTUATION or t.spelling != "*":
            continue
        off = t.extent.start.offset
        if not game(off):
            continue
        kind = carr[i].kind
        verdict = is_type_star(kind)
        if verdict:
            edits.append((off, 1, P32, 0))
        elif verdict is None and stats is not None:
            line = data[data.rfind(b"\n", 0, off) + 1:data.find(b"\n", off)].decode("latin-1").strip()
            stats.setdefault(str(kind), []).append(line[:110])
    data = apply(data, edits)
    # pass 2, on the result: calls through function pointers; pointers in variable argument lists
    game = game_test(data)
    tu = parse(data, args)
    edits = []
    def text(c):
        return data[c.extent.start.offset:c.extent.end.offset].decode("latin-1")
    def strip(c):
        while c.kind in (K.UNEXPOSED_EXPR, K.PAREN_EXPR):
            ch = list(c.get_children())
            if len(ch) != 1:
                break
            c = ch[0]
        return c
    def walk(c):
        for ch in c.get_children():
            walk(ch)
        if c.kind != K.CALL_EXPR or not game(c.extent.start.offset):
            return
        ch = list(c.get_children())
        if not ch:
            return
        callee, cargs = ch[0], ch[1:]
        ref = strip(callee)
        direct = ref.kind == K.DECL_REF_EXPR and ref.referenced is not None and ref.referenced.kind == K.FUNCTION_DECL
        if not direct:
            s, e = callee.extent.start.offset, callee.extent.end.offset
            edits.append((s, 0, "((__typeof__(*(" + text(callee) + ")) *)(", 1))
            edits.append((e, 0, "))", -1))
            return
        ft = ref.referenced.type
        if ft.kind == TypeKind.FUNCTIONPROTO and ft.is_function_variadic():
            fixed = len(list(ft.argument_types()))
            for a in cargs[fixed:]:
                if a.type.get_canonical().kind in (TypeKind.POINTER, TypeKind.CONSTANTARRAY, TypeKind.INCOMPLETEARRAY):
                    edits.append((a.extent.start.offset, 0, "((void * __ptr64)(", 1))
                    edits.append((a.extent.end.offset, 0, "))", -1))
    walk(tu.cursor)
    data = apply(data, edits)
    return r2l(data, args).decode("latin-1")

CONST = re.compile(rb'[\s(]*-?(0[xX][0-9a-fA-F]+|\d+)[uUlL]*[\s)]*')
SIDE = re.compile(rb'\+\+|--|[^=!<>]=[^=]')

def r2l(data, args):
    """Pass 3: call arguments evaluated from right to left, as the game's compiler (and gcc on 32-bit x86) does and
    clang does not. The game's code depends on it in places, e.g. `f(chr, kind, Get(chr, &kind)->speed)`, where
    clang read `kind` before the call that sets it and, with the value undefined, dropped the rest of the function.
        f(a, b, c)   ->   ({ __auto_type t3 = (c); __auto_type t2 = (b); __auto_type t1 = (a); f(t1, t2, t3); })
    Only calls with two or more arguments of which at least one has a side effect (a call, an assignment, ++ / --).
    Innermost calls first, one parse per nesting level."""
    count = 0
    for _ in range(12):
        game = game_test(data)
        tu = parse(data, args)
        found = []  # (start, end, callee end, [(arg start, arg end)]) of qualifying calls
        def has_call(c):
            return c.kind == K.CALL_EXPR or any(has_call(ch) for ch in c.get_children())
        def walk(c):
            inner = False
            for ch in c.get_children():
                inner |= walk(ch)
            if c.kind != K.CALL_EXPR or inner or not game(c.extent.start.offset):
                return inner
            ch = list(c.get_children())
            if len(ch) < 3:
                return inner
            spans = [(a.extent.start.offset, a.extent.end.offset) for a in ch[1:]]
            if any(e <= s for s, e in spans) or spans[0][0] < ch[0].extent.end.offset:
                return inner
            if not any(has_call(a) or SIDE.search(data[s:e]) for a, (s, e) in zip(ch[1:], spans)):
                return inner
            found.append((c.extent.start.offset, c.extent.end.offset, ch[0].extent.end.offset, spans))
            return True
        walk(tu.cursor)
        if not found:
            break
        edits = []
        for start, end, callee_end, spans in found:
            names = []
            decl = ""
            for s, e in reversed(spans):
                if CONST.fullmatch(data[s:e]):  # a number stays where it is (a 0 may be a null pointer constant)
                    names.append(data[s:e].decode("latin-1"))
                    continue
                count += 1
                names.append(f"__r2l{count}")
                decl += f"__auto_type __r2l{count} = ({data[s:e].decode('latin-1')}); "
            call = data[start:callee_end].decode("latin-1") + "(" + ", ".join(reversed(names)) + ")"
            edits.append((start, end - start, "({ " + decl + call + "; })", 0))
        data = apply(data, edits)
    return data

if __name__ == "__main__":
    st = {}
    sys.stdout.write(transform(sys.argv[1], sys.argv[2:], st))
    print(st, file=sys.stderr)
