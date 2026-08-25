#!/usr/bin/env python3
"""Which unwritten functions could be started TODAY, and which are blocked.

WHY THIS EXISTS

`worklist.py` says what is left and `closure.py --batch` says whether a set
someone already chose is closed.  Neither answers the question an ORDER needs:
of the 969 symbols left, which ones need nothing that is not already written?

Work lands in closed batches -- a set where every dependency of every member
is in the set or already written -- because one undefined symbol does not fail
one test, it fails all 92 binaries and `make` stops at the first, naming
nothing involved.  So the useful ordering signal is per symbol:

    ready      its closure needs nothing unwritten except itself.  It can be
               opened now, alone, and the tree will still link.
    blocked    its closure needs N other unwritten symbols.  Those are named,
               because a blocked symbol plus its blockers is itself a closed
               batch and is often the right unit to take.

The ready set is not a recommendation about VALUE -- a 12-byte accessor and a
9 KB equaliser method are both ready.  Read it with the byte counts.

WHAT IT DOES NOT KNOW

The same things `closure.py` does not know, because it uses its graph: this
is the LINK closure, deliberately pessimistic about run-time reachability,
and it cannot see a member GCC inlined out of existence (finding F64).  It
also inherits the walk's stop-at-written rule (finding F330), which is correct
here: a dependency we have written is a dependency that is satisfied.
"""
import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

ap = argparse.ArgumentParser(
    description="Which unwritten symbols are startable today.")
ap.add_argument("--tumap", default="build/tumap.json")
ap.add_argument("--obj", default="ref/slmodemd/dsplibs.o",
                help="the blob; also exported as BLOB for closure.py")
ap.add_argument("--span", help="only this translation-unit span")
ap.add_argument("--kind", default="call",
                help="call, data, rodata, template, or all")
ap.add_argument("--limit", type=int, default=40)
args = ap.parse_args()

# `closure.py` reads its blob path from $BLOB AT IMPORT TIME, so --obj has to
# reach the environment before the import or the two halves of this tool read
# two different objects -- silently, because both would still produce a
# report.  Parsing argv first is what makes one flag mean one thing.
os.environ.setdefault("BLOB", args.obj)
os.environ["BLOB"] = args.obj

# ORDER MATTERS BELOW, and getting it wrong fails in a way that names nothing
# useful.  `closure.py` removes tools/ from sys.path at import time -- that is
# the documented fix for `tools/dis.py` shadowing the standard library's `dis`,
# which `inspect` imports and pyelftools needs.  The side effect is that any
# tools module imported AFTER it is no longer findable: `import coverage` then
# resolves to the system code-coverage package, and the first symptom is
# `module 'coverage' has no attribute 'blob_addresses'`.
#
# So `coverage` is imported FIRST, while the path still has tools/ on it, and
# lands in sys.modules where the later import cannot miss it.
import coverage                                          # noqa: E402
import closure                                           # noqa: E402


def main():
    if not os.path.exists(args.obj):
        sys.exit("readyqueue.py: no blob at %s.  The default is RELATIVE to\n"
                 "the tree root; pass --obj with a full path when running\n"
                 "from elsewhere." % args.obj)

    syms, sec, edges = closure.build_graph()
    # THE GUARD THAT USED TO BE HERE has moved inside `closure.ours()`, which
    # refuses on an empty object tree for all three of its callers and names
    # the target that actually fills it.  It said "build first" and the tree
    # it was reading, build/src, stopped being built by a plain `make` at
    # #164 -- so the advice was right in 2018 and wrong since.  Findings F3055
    # and 3110; tools/objtree.py.
    have = closure.ours()

    addr = coverage.blob_addresses(args.obj)
    tus = coverage.load_tus(args.tumap)

    unwritten = [n for n in syms if n not in have]
    if args.kind != "all":
        unwritten = [n for n in unwritten
                     if closure.kind_of(syms, sec, n) == args.kind]

    rows = []
    for n in unwritten:
        reached = closure.expand([n], syms, edges, have)
        needs = sorted(x for x in reached if x not in have and x != n)
        span = coverage.area_of(addr.get(n, -1), tus, syms[n][1])
        if args.span and args.span not in span:
            continue
        rows.append((len(needs), syms[n][0], n, span, needs))

    ready = [r for r in rows if r[0] == 0]
    blocked = [r for r in rows if r[0] > 0]

    print("of %d unwritten %s symbols%s:"
          % (len(rows), args.kind, " in %s" % args.span if args.span else ""))
    print("  %d READY   -- closure needs nothing unwritten but itself, "
          "%d bytes" % (len(ready), sum(r[1] for r in ready)))
    print("  %d BLOCKED -- %d bytes"
          % (len(blocked), sum(r[1] for r in blocked)))
    print()

    by_span = collections.Counter()
    by_span_b = collections.Counter()
    for r in ready:
        by_span[r[3]] += r[1]
    for r in blocked:
        by_span_b[r[3]] += r[1]
    print("READY bytes by span:")
    for span, b in by_span.most_common():
        print("  %-28s %8d ready   %8d blocked" % (span, b, by_span_b[span]))
    print()

    print("READY, largest first:")
    for _n, size, name, span, _needs in sorted(ready, key=lambda r: -r[1])[:args.limit]:
        print("  %7d  %-58s %s" % (size, name[:58], span))
    print()

    print("BLOCKED, largest first (and what each still needs):")
    for cnt, size, name, _span, needs in sorted(blocked,
                                                key=lambda r: -r[1])[:args.limit]:
        print("  %7d  %-52s needs %d" % (size, name[:52], cnt))
        for d in needs[:4]:
            print("             %s" % d)
        if len(needs) > 4:
            print("             ... %d more" % (len(needs) - 4))


if __name__ == "__main__":
    main()
