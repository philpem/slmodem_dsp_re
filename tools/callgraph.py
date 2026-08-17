#!/usr/bin/env python3
"""
Who calls whom in dsplibs.o, and what that says about the order to work in.

WHY THIS EXISTS

Reconstructing `receiver` was started twice.  Both times it turned out to be
blocked -- it calls `decoderv34`, which calls `demapFrame`, which needs
`decodeDepth`, `putFrame` and `shellDemapper` first -- and both times that
was discovered by writing code and hitting a link error, not before starting.

The blockage is not subtle and not expensive to find.  Every call is a
R_386_PC32 relocation in .rel.text; ten seconds of reading them would have
answered it.  This tool reads them.

WHAT A "BLOCKED" FUNCTION IS

The differential harness renames every global in the blob to `ref_*` so the
two sides can be linked together.  That means a reconstructed function cannot
call the blob's copy of anything: if `f` calls `g`, then `g` must exist in
this tree before `f` can be linked, let alone tested.  So the reconstruction
order is a topological sort of the call graph, and anything whose callees are
missing is not "next", however small it looks.

Local (file-static) callees count too, and are worse: they have no `ref_`
alias at all, so they can only ever be driven through a caller.

WHAT IT PRINTS

  --ready       functions all of whose callees already exist here.  This is
                the work queue: everything in it can be written AND tested
                today.  Sorted by size, since small ones close faster.
  --blocked     the rest, each with the callees that are missing.
  --order       a full topological order, so a module can be planned end to
                end rather than one function at a time.
  --dot         graphviz, for looking at a subsystem's shape.
  --pairs       functions whose names suggest an encode/decode counterpart
                (map/demap, encode/decode, get/put, mod/demod, scramble/
                descramble).  Reconstructing both halves of a pair buys a
                round-trip test, which is an oracle INDEPENDENT of the blob:
                `decode(encode(x)) == x` has to hold for reasons that have
                nothing to do with matching the reference byte for byte.
                It does not replace the differential test -- two functions
                sharing one wrong table still round-trip -- so it is an
                addition, never a substitute.

CAVEAT

Indirect calls are invisible.  `putFrame` reaches its bit sink through a
pointer in the object, and nothing in .rel.text records that; the same will
be true of any dispatch table.  A function reported ready can still surprise
you, so the tool prints how many indirect calls each function makes.
"""

import argparse
import os
import re
import subprocess
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import objtree                                            # noqa: E402


def run(*cmd):
    out = subprocess.run(cmd, capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit("failed: %s\n%s" % (" ".join(cmd), out.stderr))
    return out.stdout


def symbols(obj):
    """Defined text symbols: name -> (addr, size, 'T' global or 't' local)."""
    syms = {}
    for line in run("nm", "-S", "--defined-only", obj).split("\n"):
        f = line.split()
        if len(f) >= 4 and f[2] in "Tt":
            syms[f[3]] = (int(f[0], 16), int(f[1], 16), f[2])
    return syms


def owner(syms, addr):
    """Which function contains `addr`."""
    best = None
    for name, (a, size, _) in syms.items():
        if a <= addr < a + size:
            return name
        if a <= addr and (best is None or a > syms[best][0]):
            best = name
    return best


def call_edges(obj, syms):
    """caller -> set(callee), plus caller -> count of indirect calls."""
    edges = defaultdict(set)
    indirect = defaultdict(int)

    # Relocations give the direct calls.
    text = run("objdump", "-dr", "--section=.text", obj)
    cur = None
    for line in text.split("\n"):
        m = re.match(r"^([0-9a-f]+) <([^>]+)>:", line)
        if m:
            cur = m.group(2)
            continue
        if cur is None:
            continue
        m = re.search(r"R_386_PC32\s+(\S+)", line)
        if m:
            callee = m.group(1)
            if callee in syms and callee != cur:
                edges[cur].add(callee)
            elif callee not in syms:
                edges[cur].add(callee)          # an import, e.g. sysdep_*
            continue
        if re.search(r"\bcall\s+\*", line):
            indirect[cur] += 1

    # Local callees are called by address, with no relocation naming them.
    for line in text.split("\n"):
        m = re.match(r"^([0-9a-f]+) <([^>]+)>:", line)
        if m:
            cur = m.group(2)
            continue
        m = re.search(r"\bcall\s+([0-9a-f]+) <([^+>]+)(\+0x[0-9a-f]+)?>", line)
        if m and cur:
            callee = m.group(2)
            if callee in syms and callee != cur:
                edges[cur].add(callee)

    return edges, indirect


def reconstructed(paths=None):
    r"""Function names that already exist in this tree, from the OBJECTS.

    This used to grep `src/**/*.c` for `^(\w+)\s*\(` -- a name at column 0
    followed by a parenthesis.  Two things were wrong with that, and the
    second went unnoticed for the whole of the C++ work:

      it counts a prototype, a comment or a K&R declaration as a definition,
      which is the exact mistake coverage.py's docstring warns against;

      it cannot see C++ AT ALL.  A member is written `Class::method(`, which
      the regex does not match, and even if it did the blob's symbols are
      mangled and the source spells them out.  So every C++ function this
      tree has written counted as missing, and `--of v34handshak` went on
      reporting 16,003 bytes of unwritten C++ after eight batches of it had
      landed.

    Read the built objects instead, as coverage.py and closure.py both do.
    Requires a build; that is a fair price for an answer that is true.

    AND IT REFUSES WHEN THERE ARE NONE.  The warning that used to be here
    named `make`, went to stderr, and was followed by the report anyway at
    exit 0 -- so `--ready` listed all 73 classes' methods as writable-now and
    `--of v34handshak` counted every written function as missing, which is
    the answer this docstring's second paragraph was written to end.  A
    warning nobody has to act on is the same as no warning: findings 134,
    3055 and 3110, and tools/objtree.py for the directories probed.
    """
    have = set()
    if paths:
        objs = paths
        sys.stderr.write("callgraph.py: read %d object(s) named on the "
                         "command line\n" % len(objs))
    else:
        _d, objs = objtree.read("which functions this tree has written")
    for o in objs:
        for line in run("nm", "--defined-only", o).split("\n"):
            f = line.split()
            if len(f) >= 3:
                have.add(f[-1])
    return have


PAIR_RULES = [
    ("demap", "map"), ("decode", "encode"), ("descramble", "scramble"),
    ("demod", "mod"), ("get", "put"), ("rx", "tx"), ("unpack", "pack"),
]


def find_pairs(syms):
    """Names that look like two halves of one codec."""
    lower = {n.lower(): n for n in syms}
    pairs = []
    seen = set()
    for name in sorted(syms):
        low = name.lower()
        for a, b in PAIR_RULES:
            for src, dst in ((a, b), (b, a)):
                if src in low:
                    other = low.replace(src, dst, 1)
                    if other in lower and other != low:
                        key = tuple(sorted((low, other)))
                        if key not in seen:
                            seen.add(key)
                            pairs.append((name, lower[other]))
    return pairs


def main():
    ap = argparse.ArgumentParser(
        description="Call graph of dsplibs.o, and the reconstruction order "
                    "it implies.")
    ap.add_argument("--obj", default="ref/slmodemd/dsplibs.o")
    ap.add_argument("--src", nargs="*", default=None,
                    help="sources to scan for what is already reconstructed; "
                         "defaults to src/**/*.c")
    ap.add_argument("--filter", default=None,
                    help="only functions whose name matches this regex")
    ap.add_argument("--ready", action="store_true")
    ap.add_argument("--blocked", action="store_true")
    ap.add_argument("--order", action="store_true")
    ap.add_argument("--pairs", action="store_true")
    ap.add_argument("--dot", action="store_true")
    ap.add_argument("--of", default=None,
                    help="restrict to what this function reaches, "
                         "transitively")
    args = ap.parse_args()

    syms = symbols(args.obj)
    edges, indirect = call_edges(args.obj, syms)

    have = reconstructed(args.src)

    keep = set(syms)
    if args.of:
        keep, todo = set(), [args.of]
        while todo:
            n = todo.pop()
            if n in keep or n not in syms:
                continue
            keep.add(n)
            todo.extend(edges.get(n, ()))
    if args.filter:
        rx = re.compile(args.filter)
        keep = {n for n in keep if rx.search(n)}

    def missing(name):
        return sorted(c for c in edges.get(name, ())
                      if c in syms and c not in have)

    if args.pairs:
        print("Encode/decode pairs -- reconstructing both buys a round-trip")
        print("test, an oracle independent of the blob.\n")
        for a, b in find_pairs(syms):
            ha = "have" if a in have else "    "
            hb = "have" if b in have else "    "
            print("  %-28s %s   <->   %-28s %s"
                  % (a, ha, b, hb))
        return

    if args.dot:
        print("digraph calls {")
        print('  node [shape=box, fontname="monospace"];')
        for n in sorted(keep):
            style = "" if n in have else ', style=filled, fillcolor="#ffdddd"'
            print('  "%s" [label="%s\\n%d B"%s];'
                  % (n, n, syms[n][1], style))
            for c in sorted(edges.get(n, ())):
                if c in keep:
                    print('  "%s" -> "%s";' % (n, c))
        print("}")
        return

    if args.ready or not (args.blocked or args.order):
        rows = [(syms[n][1], n) for n in keep
                if n not in have and not missing(n)]
        rows.sort()
        print("READY -- every callee already exists here, so these can be "
              "written and tested now:\n")
        for size, n in rows:
            ind = indirect.get(n, 0)
            note = "  (%d indirect call%s)" % (ind, "" if ind == 1 else "s") \
                   if ind else ""
            print("  %6d B  %-3s %-32s%s"
                  % (size, syms[n][2], n, note))
        print("\n  %d ready, %d bytes" % (len(rows), sum(r[0] for r in rows)))

    if args.blocked:
        rows = [(syms[n][1], n, missing(n)) for n in keep
                if n not in have and missing(n)]
        rows.sort()
        print("\nBLOCKED -- and on what:\n")
        for size, n, miss in rows:
            print("  %6d B  %-3s %-28s needs %s"
                  % (size, syms[n][2], n, ", ".join(miss)))
        print("\n  %d blocked, %d bytes" % (len(rows), sum(r[0] for r in rows)))

    if args.order:
        # Kahn, callees first, ties broken by size so small ones come out
        # early and close faster.
        indeg = {n: 0 for n in keep}
        rdeps = defaultdict(set)
        for n in keep:
            for c in edges.get(n, ()):
                if c in keep and c != n:
                    indeg[n] += 1
                    rdeps[c].add(n)
        ready = sorted((syms[n][1], n) for n in keep if indeg[n] == 0)
        out = []
        while ready:
            size, n = ready.pop(0)
            out.append(n)
            for d in sorted(rdeps[n]):
                indeg[d] -= 1
                if indeg[d] == 0:
                    ready.append((syms[d][1], d))
                    ready.sort()
        print("\nORDER -- callees before callers:\n")
        for n in out:
            mark = "have" if n in have else "    "
            print("  %s  %6d B  %-3s %s" % (mark, syms[n][1], syms[n][2], n))
        cyc = [n for n in keep if n not in out]
        if cyc:
            print("\n  %d in cycles (mutual recursion), not ordered: %s"
                  % (len(cyc), ", ".join(sorted(cyc))))


if __name__ == "__main__":
    main()
