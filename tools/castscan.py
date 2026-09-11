#!/usr/bin/env python3
"""
Audit explicit numeric casts that the surrounding context already performs.

Two classes, both no-ops in the C abstract machine, neither of them the
"forced narrowing" that `docs/cleanup.md` F3a says to keep:

  identity   -- the operand already has the cast's type (`(int)i` on an int)
  contextual -- the cast target equals the type the context converts to
                anyway: the initializer, the assignment destination, the
                return type, or the peer of a floating binary/conditional
                expression (`long double x = (long double)v;`)

The contextual class is exactly what finding F11353 removed for `long double`
(`frac_of`) and what finding F10163 named "documentation-only narrowing
casts".

Soundness rules, and they are load-bearing:

  * arithmetic to arithmetic only.  A pointer cast can be the thing that makes
    the code legal at all, so it is never "the context already does this".
  * `binary-peer` is restricted to FLOATING targets.  C's integer promotions
    mean `(short)a * b` still truncates `a` even when `b` is `short`.
  * init/assign require the cast target to EQUAL the destination.  This does
    NOT flag `int x = (short)unsigned_val;` -- there the target differs from
    the destination and the cast genuinely changes the value by sign-extending.
  * macro-body casts and system/project headers are reported separately.  An
    identity result inside a generic macro (`FIELD`'s `(void *)`) is an
    artefact of one instantiation, not a defect.

An AST via clang is the only accurate way to do this; a regex cannot resolve
the operand type.  clang is analysis apparatus, never an authority for
reconstruction -- a candidate is accepted only after the period compiler
reproduces the whole object byte for byte (finding F11353).

Usage:
    castscan.py [FILE ...]         # default: git ls-files 'src/**/*.c|.cpp'
    castscan.py --by-file
    castscan.py -v
    castscan.py --self-test
"""

import argparse
import json
import re
import subprocess
from collections import Counter
from concurrent.futures import ProcessPoolExecutor

FLOAT = {"float", "double", "long double"}
_ARITH = re.compile(
    r"^(?:(?:(?:un)?signed )?"
    r"(?:char|short(?: int)?|int|long(?: long)?(?: int)?|_Bool|bool)"
    r"|float|double|long double)$")


def norm(t):
    t = re.sub(r"\b(class|struct|enum)\b", "", t)
    t = re.sub(r"\b(const|volatile|restrict|__restrict)\b", "", t)
    return re.sub(r"\s+", " ", t).strip()


def desug(tnode):
    return norm(tnode.get("desugaredQualType") or tnode.get("qualType", ""))


def is_arith(t):
    return bool(_ARITH.match(t))


def is_float(t):
    return norm(t) in FLOAT


def signedness(t):
    """'signed' / 'unsigned' / 'float' / None for a normalized type."""
    if t in FLOAT:
        return "float"
    if not is_arith(t):
        return None
    if t.startswith("unsigned") or t in ("_Bool", "bool"):
        return "unsigned"
    return "signed"


def operand_type(node, target):
    """Type of the operand BEFORE the cast's own conversion.

    A CStyleCastExpr nests its semantic conversion as an ImplicitCastExpr
    whose type is already the target; descend that chain (type == target) and
    stop at the first node whose type differs.  Comparing the target to
    anything on the conversion chain would call every cast an identity.
    """
    inner = node.get("inner")
    if not inner:
        return ""
    x = inner[0]
    while True:
        if x.get("kind") == "ParenExpr" and x.get("inner"):
            x = x["inner"][0]
            continue
        t = desug(x.get("type", {}))
        if (x.get("kind") == "ImplicitCastExpr" and x.get("inner")
                and t == norm(target)):
            x = x["inner"][0]
            continue
        return t


def context_kind(cast, anc, T, O):
    """Why the context already converts to T, or None."""
    if not (is_arith(T) and is_arith(O)):
        return None
    parent = anc[-1]
    pk = parent.get("kind")
    inner = parent.get("inner", [])
    if pk == "VarDecl" and desug(parent.get("type", {})) == norm(T):
        return "init"
    if (pk == "BinaryOperator" and parent.get("opcode") == "="
            and len(inner) > 1 and inner[1] is cast
            and desug(inner[0].get("type", {})) == norm(T)):
        return "assign"
    if pk == "BinaryOperator" and parent.get("opcode") != "=" and is_float(T):
        for sib in inner:
            if sib is not cast and desug(sib.get("type", {})) == norm(T):
                return "binary-peer"
    if pk == "CStyleCastExpr" and desug(parent.get("type", {})) == norm(T):
        return "nested"
    if pk == "ConditionalOperator" and is_float(T):
        for sib in inner:
            if sib is not cast and desug(sib.get("type", {})) == norm(T):
                return "cond-peer"
    if pk == "ReturnStmt":
        for a in reversed(anc):
            if a.get("kind") == "FunctionDecl":
                return "return" if desug(a.get("type", {})) == norm(T) else None
    return None


def scan(path):
    is_cpp = path.endswith((".cpp", ".hpp"))
    cc = "clang++" if is_cpp else "clang"
    std = "-std=gnu++98" if is_cpp else "-std=gnu89"
    proc = subprocess.run(
        [cc, std, "-fsyntax-only", "-Xclang", "-ast-dump=json",
         "-Iinclude", "-I.", path],
        capture_output=True, text=True)
    if proc.returncode != 0 and '"kind"' not in proc.stdout:
        return path, "parse-failed", []
    try:
        tree = json.loads(proc.stdout)
    except ValueError:
        return path, "parse-failed", []
    text = open(path, encoding="utf-8", errors="replace").read()
    hits = []

    def walk(n, anc):
        if isinstance(n, list):
            for x in n:
                walk(x, anc)
            return
        if not isinstance(n, dict):
            return
        if n.get("kind") == "CStyleCastExpr":
            raw = n.get("type", {}).get("qualType", "")
            T = desug(n.get("type", {}))
            O = operand_type(n, T)
            if T and O:
                b = n.get("range", {}).get("begin") or {}
                sl = b.get("spellingLoc", b)
                f = sl.get("file") or sl.get("includedFrom", {}).get("file")
                macro = "expansionLoc" in b
                line = sl.get("line")
                if line is None and "offset" in b:
                    line = text.count("\n", 0, b["offset"]) + 1
                if T == norm(O):
                    hits.append((path, "identity", raw, T, f, macro, line, O))
                elif anc:
                    k = context_kind(n, anc, T, O)
                    if k:
                        hits.append((path, k, raw, T, f, macro, line, O))
        for v in n.values():
            if isinstance(v, (list, dict)):
                walk(v, anc + [n])

    walk(tree, [])
    return path, "ok", hits


def collect(files):
    hits, fail = [], []
    with ProcessPoolExecutor(max_workers=8) as ex:
        for path, status, h in ex.map(scan, files):
            if status != "ok":
                fail.append(path)
            hits.extend(h)
    return hits, fail


def default_files():
    return subprocess.run(["git", "ls-files", "src/**/*.c", "src/**/*.cpp"],
                          capture_output=True, text=True).stdout.split()


def self_test():
    """Show both detectors firing and the two soundness boundaries holding."""
    selftest = "/tmp/castscan_selftest.c"
    with open(selftest, "w") as fh:
        fh.write(
            "long double f(float v, long double x, float scale)\n"
            "{\n"
            "\tlong double d = (long double)v;\n"           # ctx init
            "\tint n = (int)(d * (long double)scale);\n"    # ctx init + peer
            "\tlong double y = (long double)(int)v - x;\n"  # ctx peer
            "\tint m = (int)d;\n"                           # ctx init
            "\t(void)n; (void)y; (void)m;\n"
            "\treturn (long double)v + d;\n"                # ctx peer
            "}\n"
            "int g(short s, int i, unsigned short u)\n"
            "{\n"
            "\tint a = (int)i;\n"                           # identity
            "\tshort b = (short)s;\n"                       # identity
            "\tint c = (int)s;\n"                           # ctx init
            "\tint p = (short)u;\n"                         # NOT: T != D
            "\tlong z = (short)i * s;\n"                    # NOT: promotion
            "\t(void)a;(void)b;(void)c;(void)p;(void)z;\n"
            "\treturn 0;\n"
            "}\n")
    hits, _ = collect([selftest])
    hits = [h for h in hits if h[4] is None]  # main file only
    kinds = Counter(h[1] for h in hits)
    ident = kinds["identity"]
    ctx = sum(v for k, v in kinds.items() if k != "identity")
    assert ident == 2, ("identity", ident, hits)
    assert ctx == 7, ("contextual", ctx, hits)
    print("castscan self-test: %d hits checked (2 identity, 7 contextual); "
          "rejected the T!=D cast and the integer-promotion cast" % len(hits))


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*")
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--by-file", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--all", action="store_true",
                    help="include macro-body and header casts")
    ap.add_argument("--crossing", action="store_true",
                    help="list the signedness-crossing candidates")
    args = ap.parse_args()

    if args.self_test:
        self_test()
        return

    files = args.files or default_files()
    hits, fail = collect(files)
    scoped = [h for h in hits if args.all or h[4] is None
              or "src/" in (h[4] or "").replace("\\", "/")]
    direct = [h for h in scoped if not h[5]]
    macro = [h for h in scoped if h[5]]

    print("files scanned : %d  (%d parse-failed)" % (len(files), len(fail)))
    print("direct casts  : %d   (in macros: %d)" % (len(direct), len(macro)))
    ident = [h for h in direct if h[1] == "identity"]
    ctx = [h for h in direct if h[1] != "identity"]
    print("identity   : %d" % len(ident))
    for t, c in Counter(h[3] for h in ident).most_common():
        print("   %-26s %d" % (t, c))
    print("contextual : %d" % len(ctx))
    for k, c in Counter(h[1] for h in ctx).most_common():
        print("   by context: %-14s %d" % (k, c))
    for t, c in Counter(h[3] for h in ctx).most_common():
        print("   %-26s %d" % (t, c))
    cross = [h for h in ctx
             if {signedness(h[3]), signedness(h[7])} == {"signed", "unsigned"}]
    print("   signedness-crossing: %d  (conversion is identical either way; "
          "the cast only hides -Wsign-conversion)" % len(cross))
    if args.crossing:
        for path, kind, raw, T, f, m, line, O in sorted(
                cross, key=lambda h: (h[0], h[6] or 0)):
            print("  %-11s %s:%s  (%s <- %s)"
                  % (kind, path, line, T, O))
    mi = [h for h in macro if h[1] == "identity"]
    if mi:
        print("identity in macros (not defects): %d" % len(mi))
        for t, c in Counter(h[3] for h in mi).most_common():
            print("   %-26s %d" % (t, c))
    if args.by_file:
        print("contextual by file:")
        for f, c in Counter(h[0] for h in ctx).most_common(25):
            print("   %-44s %d" % (f, c))
    if args.verbose:
        for path, kind, raw, T, f, m, line, O in sorted(
                direct, key=lambda h: (h[1], h[0], h[6] or 0)):
            print("  %-11s %s:%s  (%s)" % (kind, path, line, raw))


if __name__ == "__main__":
    main()
