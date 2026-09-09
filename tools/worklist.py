#!/usr/bin/env python3
"""Every function still to be reconstructed, named, sized and placed.

WHY THIS EXISTS

`coverage.py` says how much is left as a percentage and `closure.py` says
what one batch must define before it will link.  Neither ENUMERATES the work:
"38.8%" does not tell anyone which function to open next, and a closure is
scoped to one entry point and stops at everything already written.

This lists the remainder, one row per symbol, grouped by the translation-unit
span it came from -- because a TU is the unit the original was written in and
the unit a batch can sensibly take, and because the span comes from
`tumap.json`, which is measured from the object rather than inferred from a
call graph.

THE THREE CATEGORIES, AND WHY THE THIRD IS NOT OPTIONAL

  not written        the blob defines the symbol and our own built objects
                     -- `build/src`, or `build/repro` when a plain `make` is
                     all that has been run -- do not.  A set difference;
                     exact.  An EMPTY object tree is refused rather than
                     reported as "nothing written": findings F3055 and F3110.

  written            a function of that name exists on our side.  Note what
                     this does NOT claim: `coverage.py`'s own docstring says
                     it counts "a function of the same name", so a name is
                     the whole test.

  written, with      a function that exists, links, passes its differential
  unreconstructed    test, and still routes some of its arms into a
  regions            `*_notwritten()` stub.  `v34handshak` is the reason the
                     convention exists (finding F547): it is one symbol, so
                     the set difference calls it DONE, and the arms behind
                     the stub are invisible to every count taken per symbol.

The third category cannot be derived from symbol tables at all -- inlining
means the missing arms have no symbol to be missing.  It is read out of the
source, from the stub convention the tree already uses.

WHAT IT DOES NOT KNOW

Whether the blob's version of a symbol we HAVE written is fully covered.  A
count per symbol cannot see inside one; `debugaudit.py --missing` is the
per-function view, with the caveat that a missing diagnostic call is not by
itself an unreconstructed region -- the level ships at zero, so the original's
`edprintf` sites were often simply not reproduced (finding F134).
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coverage                                          # noqa: E402


# The stub convention, one entry per pump that uses it.  Each names the
# recording function to grep for and the header its codes are spelled in, so
# a new pump adopting the convention is one line here.
STUBS = [
    ("src/pump/v34/V34hshak.c",     r"t3c_unwritten\(\)",   "T3M_"),
    ("src/pump/v34/v34pcmmain.cpp", r"v34pcm_notwritten\(", "V34PCM_"),
    ("src/pump/v90/vpcm.c",         r"vpcm_notwritten\(",   "VPCM_"),
]

# A definition at column 0, which is this tree's style throughout.
DEF = re.compile(r"^([A-Za-z_][A-Za-z0-9_:<>~ \*&]*?)\s*\(")


def stub_sites(root):
    """[(file, line, enclosing function, text)] for every live stub call.

    The DEFINITION of the recorder is skipped -- it matches its own pattern --
    and so is anything inside a comment, so a retired code still named in
    prose does not read as an open arm.
    """
    out = []
    for rel, pat, _prefix in STUBS:
        path = os.path.join(root, rel)
        try:
            with open(path, encoding="utf-8", errors="replace") as fh:
                lines = fh.read().splitlines()
        except OSError:
            continue
        rx = re.compile(pat)
        for i, ln in enumerate(lines):
            if not rx.search(ln):
                continue
            s = ln.strip()
            if s.startswith(("*", "/*", "//")):
                continue
            fn = "?"
            for j in range(i, max(0, i - 900), -1):
                if lines[j].startswith(("#", "/", " ", "\t")):
                    continue
                m = DEF.match(lines[j])
                if m and m.group(1).strip() not in (
                        "if", "for", "while", "switch", "return"):
                    fn = m.group(1).strip()
                    break
            if fn == s.split("(")[0]:       # the recorder defining itself
                continue
            out.append((rel, i + 1, fn, s))
    return out


def main():
    ap = argparse.ArgumentParser(
        description="Enumerate the functions still to be reconstructed.")
    ap.add_argument("--obj", default="ref/slmodemd/dsplibs.o")
    ap.add_argument("--build", default="build")
    ap.add_argument("--tumap", default="build/tumap.json")
    ap.add_argument("--root", default=".",
                    help="tree to read the stub convention out of")
    ap.add_argument("--md", help="write the report here instead of stdout")
    ap.add_argument("--limit", type=int, default=0,
                    help="rows per span, 0 for all")
    args = ap.parse_args()

    blob = coverage.nm_symbols(args.obj)
    addr = coverage.blob_addresses(args.obj)
    ours = coverage.our_symbols(args.build)
    tus = coverage.load_tus(args.tumap)

    # An unreadable --obj is the same failure from the other end: `nm` writes
    # to stderr, returns nothing, and every count comes out ZERO rather than
    # wrong-looking.  The default is relative, so running this from anywhere
    # but the tree root hits it.  Refuse.
    if not blob:
        sys.exit("worklist.py: %s defines no T/t/W symbols -- unreadable, or\n"
                 "not the blob.  The default is RELATIVE to the tree root;\n"
                 "pass --obj with a full path when running from elsewhere."
                 % args.obj)

    # THE GUARD THAT USED TO BE HERE has moved inside `coverage.our_symbols()`,
    # which both this and coverage.py read the have-set through.  Its advice
    # was "Build first", and the directory it meant -- build/src -- stopped
    # being filled by a plain `make` at #164, so following it changed nothing
    # and the report stayed wrong.  objtree.read() now names `make coverage`
    # and prints the object count.  Findings F3055 and F3110.
    if not tus:
        sys.exit("worklist.py: no TU map at %s, so nothing can be attributed\n"
                 "to a translation unit.  `make %s` builds it."
                 % (args.tumap, args.tumap))

    rows = []
    for name, (size, kind) in blob.items():
        if name in ours:
            continue
        rows.append((coverage.area_of(addr.get(name, -1), tus, kind),
                     size, name, kind))

    by_span = {}
    for span, size, name, kind in rows:
        by_span.setdefault(span, []).append((size, name, kind))

    lines = []

    def add(s=""):
        lines.append(s)

    total_b = sum(r[1] for r in rows)
    written_b = sum(s for n, (s, k) in blob.items() if n in ours)

    add("dsplibs.o -- the work that is left, function by function")
    add()
    add("  %d symbols the blob defines and src/ does not, %d bytes"
        % (len(rows), total_b))
    add("  %d symbols written, %d bytes"
        % (len(blob) - len(rows), written_b))
    add()
    add("  Sizes are the BLOB's.  They size the reading, not the writing.")
    add()
    # RELATIVE, deliberately.  This file is committed, so an absolute path
    # here makes it churn on whoever regenerated it last and from where --
    # `make worklist` would show a diff every time it ran from a worktree.
    add("  The two halves have two different sources and can drift.  The")
    add("  counts are from the object and the built objects under")
    # BOTH DIRECTORIES, AND STATICALLY.  Naming the one this run happened to
    # read would make the committed file depend on which build the regenerator
    # had done -- the same churn the note above avoids.  Which was read is on
    # stderr, every run.  Findings F3055 and F3110.
    add("  %s/src or %s/repro, whichever a build has filled (the run's"
        % (os.path.relpath(args.build), os.path.relpath(args.build)))
    add("  own stderr says which); the stub sites are read from the SOURCE")
    add("  under %s -- which under `make worklist` is the working"
        % os.path.relpath(args.root))
    add("  tree, so an uncommitted edit moves those line numbers.")
    add()

    add("=" * 70)
    add("NOT WRITTEN, by translation-unit span")
    add("=" * 70)
    add()
    order = sorted(by_span.items(), key=lambda kv: -sum(x[0] for x in kv[1]))
    for span, items in order:
        items.sort(key=lambda x: (-x[0], x[1]))
        add("%s" % span)
        add("    %d symbols, %d bytes"
            % (len(items), sum(x[0] for x in items)))
        shown = items if not args.limit else items[:args.limit]
        for size, name, kind in shown:
            add("      %7d  %s%s" % (size, name, "  (weak)"
                                     if kind == "W" else ""))
        if len(shown) < len(items):
            add("      ... %d more" % (len(items) - len(shown)))
        add()

    sites = stub_sites(args.root)
    add("=" * 70)
    add("WRITTEN, WITH UNRECONSTRUCTED REGIONS")
    add("=" * 70)
    add()
    add("  Functions that exist, link and pass their differential test, and")
    add("  still route some arm into a stub.  Every one of these is counted")
    add("  as WRITTEN above and by coverage.py, because it is one symbol.")
    add()
    if not sites:
        add("  (none -- the stub convention has no live call site)")
    else:
        per_fn = {}
        for rel, line, fn, text in sites:
            per_fn.setdefault((rel, fn), []).append((line, text))
        for (rel, fn), hits in sorted(per_fn.items(),
                                      key=lambda kv: -len(kv[1])):
            size = blob.get(fn, (0, "?"))[0]
            add("  %s   %s%s"
                % (fn, rel, "   (%d bytes in the blob)" % size if size else ""))
            for line, text in sorted(hits):
                add("      %s:%d" % (rel, line))
            add()

    text = "\n".join(lines) + "\n"
    if args.md:
        with open(args.md, "w", encoding="utf-8") as fh:
            fh.write("# The work that is left\n\n"
                     "Generated by `tools/worklist.py`; run `make worklist` "
                     "to refresh.\n\n```\n")
            fh.write(text)
            fh.write("```\n")
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
