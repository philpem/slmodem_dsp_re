#!/usr/bin/env python3
"""
Who calls whom, between translation-unit spans of dsplibs.o.

WHY THIS EXISTS

Deciding what to reconstruct next needs more than a size ranking.  A 300 KB
module that nothing else reaches can be left until last; a 4 KB one that
everything depends on cannot.  "Is V.34 a prerequisite for V.90?" is a
question about edges, not bytes, and guessing it from the file names would be
exactly the kind of plausible-and-wrong the rest of this project tries to
avoid.

HOW IT WORKS

Every call and every data reference in the object is a relocation.  `objdump
-dr` gives them with the address they sit at, so each one can be attributed to
the function that contains it, and each function to a translation-unit span
via tools/tumap.py.  Aggregating over spans gives the module graph.

Spans, not files: only ~19 of the 283 TUs have local symbols to anchor them,
so the rest share a bracket with their neighbours and cannot be told apart.
An edge is therefore between *groups* of candidate files, and the label names
the first with a count of how many others share it.  That is a real limit of
the symbol table, not of this tool, and it is why the output says "+13" rather
than pretending to a precision it does not have.

Usage:
    deps.py [--obj OBJ] [--tumap build/tumap.json] [--span SUBSTRING]
    deps.py --span V34hshak      # what that span needs, and who needs it
    deps.py --range 0x5af10:0x73e20=V.34 --span V.34

`--range` carves a named region out of the span map before anything else is
attributed, which is how a bracket holding two unrelated modules is separated.
The V.34 code and the V.8 code share one bracket, so asking about "V34hshak.c
+13" answers about both at once and about neither usefully.  Give the
addresses and the two come apart.
"""

import argparse
import json
import re
import subprocess
import sys

INSN = re.compile(r"^\s*([0-9a-f]+):\t")
RELOC = re.compile(r"^\s+([0-9a-f]+):\s+(R_386_\S+)\s+(\S+)")
FUNC = re.compile(r"^([0-9a-f]+) <([^>]+)>:")


def load_spans(path):
    with open(path, encoding="utf-8") as fh:
        data = json.load(fh)
    spans = {}
    for name, t in data["tus"].items():
        spans.setdefault((t["lo"], t["hi"]), []).append((t["seq"], name))
    out = []
    for (lo, hi), members in sorted(spans.items()):
        members.sort()
        label = members[0][1]
        if len(members) > 1:
            label += " +%d" % (len(members) - 1)
        out.append((lo, hi, label, [n for _s, n in members]))
    return out


def span_of(addr, spans, override=()):
    for lo, hi, label in override:
        if lo <= addr < hi:
            return label
    for lo, hi, label, _files in spans:
        if lo <= addr < hi:
            return label
    return None


def symbol_addresses(obj):
    """{name: (address, kind)} for every defined symbol."""
    out = subprocess.run(["nm", "--defined-only", obj], capture_output=True,
                         text=True).stdout
    addr = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) == 3:
            addr[f[2]] = (int(f[0], 16), f[1])
    return addr


def build_edges(obj, spans, override=()):
    """({(from_span, to_span): {(caller, callee)}}, {from_span: {(c, t)}})

    The second return is references to *data* -- tables, mostly.  They are not
    module edges and must not be attributed as if they were: this is an `ld -r`
    object, so every section starts at zero and a `.rodata` address means
    nothing when compared against a `.text` span.  Doing that silently placed
    `V23_IIR_FILT` in the Bell 103 bracket, three hundred kilobytes from where
    it lives.  A coefficient table's owning TU comes from the STT_FILE order in
    the symbol table instead, which is a different question and answered by
    tumap.py.
    """
    text = subprocess.run(["objdump", "-dr", "-j", ".text", obj],
                          capture_output=True, text=True).stdout
    addr = symbol_addresses(obj)
    edges = {}
    data = {}
    here = None
    here_span = None
    for line in text.splitlines():
        m = FUNC.match(line)
        if m:
            here = m.group(2)
            here_span = span_of(int(m.group(1), 16), spans, override)
            continue
        m = RELOC.match(line)
        if not m or here_span is None:
            continue
        target = m.group(3)
        if target in (".rodata", ".data", ".bss", ".rodata.str1.1",
                      ".rodata.str1.4"):
            continue
        if target not in addr:
            continue            # an import: not a module edge
        where, kind = addr[target]
        if kind not in "Tt":
            data.setdefault(here_span, set()).add((here, target))
            continue
        to_span = span_of(where, spans, override)
        if to_span is None or to_span == here_span:
            continue
        edges.setdefault((here_span, to_span), set()).add((here, target))
    return edges, data


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--obj", default="ref/slmodemd/dsplibs.o")
    ap.add_argument("--tumap", default="build/tumap.json")
    ap.add_argument("--span", help="report one span's edges in both directions")
    ap.add_argument("--range", action="append", default=[],
                    metavar="LO:HI=NAME",
                    help="carve a named region out of the span map first")
    ap.add_argument("--examples", type=int, default=4,
                    help="how many symbol pairs to show per edge")
    args = ap.parse_args()

    spans = load_spans(args.tumap)
    override = []
    for spec in args.range:
        span, _, name = spec.partition("=")
        lo, _, hi = span.partition(":")
        override.append((int(lo, 0), int(hi, 0), name or span))
    edges, data = build_edges(args.obj, spans, override)

    if not args.span:
        counts = {}
        for (a, b), pairs in edges.items():
            counts.setdefault(a, set()).add(b)
        print("module graph: %d spans, %d edges" % (len(spans), len(edges)))
        print()
        for _lo, _hi, label, _files in spans:
            outs = sorted(counts.get(label, ()))
            ins = sorted(a for (a, b) in edges if b == label)
            print("  %-32s calls %d, called by %d"
                  % (label, len(outs), len(set(ins))))
        return 0

    want = [name for _lo, _hi, name in override if args.span in name]
    want += [lbl for _lo, _hi, lbl, files in spans
             if args.span in lbl or any(args.span in f for f in files)]
    if not want:
        print("no span matching %r" % args.span, file=sys.stderr)
        return 1

    for label in want:
        print("=" * 72)
        print(label)
        for _lo, _hi, lbl, files in spans:
            if lbl == label:
                print("  candidate files: %s" % ", ".join(files))
        print()
        print("  NEEDS (this span calls out to):")
        rows = sorted((b, pairs) for (a, b), pairs in edges.items()
                      if a == label)
        if not rows:
            print("    nothing")
        for to, pairs in rows:
            print("    %-34s %3d references" % (to, len(pairs)))
            for caller, callee in sorted(pairs)[:args.examples]:
                print("        %s -> %s" % (caller, callee))
            if len(pairs) > args.examples:
                print("        ... and %d more" % (len(pairs) - args.examples))
        print()
        pairs = sorted(data.get(label, ()))
        if pairs:
            print("  READS (data, so no span -- see the note in build_edges):")
            for caller, target in pairs:
                print("        %s -> %s" % (caller, target))
            print()
        print("  NEEDED BY (calls into this span):")
        rows = sorted((a, pairs) for (a, b), pairs in edges.items()
                      if b == label)
        if not rows:
            print("    nothing -- this span is a leaf of the module graph")
        for frm, pairs in rows:
            print("    %-34s %3d references" % (frm, len(pairs)))
            for caller, callee in sorted(pairs)[:args.examples]:
                print("        %s -> %s" % (caller, callee))
            if len(pairs) > args.examples:
                print("        ... and %d more" % (len(pairs) - args.examples))
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
