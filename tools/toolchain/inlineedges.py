#!/usr/bin/env python3
"""Screen call-edge differences that can mean an inline boundary moved.

THE QUESTION THIS ANSWERS

An inlined callee leaves no `call`, so a function can be SIZE for the one
reason that GCC inlined a helper the object kept out of line.  `compare.py`
and the call-graph tools see the source, not the boundary, and the boundary
is a FRONTEND COST decision (`max-inline-insns-auto`, ~100 in GCC 3.4.2),
not a lexeme.  #333 recovered `V92ConvolutionEncoder::process` that way: its
callee sat at cost 98 and a body-identical spelling moved it to 102.

This screen finds the same shape elsewhere.  For every symbol both builds
define it compares the multiset of DIRECT call targets, and separately of
TAIL jumps, and reports where the object makes a call that ours does not or
the reverse.  Targets already grade-0 EXACT are the interesting ones: the
callee is byte-identical, so the only difference is who contains the body.

WHAT IS REPORTED

    symbols compared        the denominator, always printed
    call edges              blob total / ours total
    missing / extra         per caller, per target, with counts
    exact?                  the target's grade-0 verdict, from --exact-list

WHAT THIS IS NOT

Not a grade and not a claim that the target should be out of line.  A missing
call can also be an inlined copy that is genuinely the object's own (the
object's `v34handshak` contains 26 Scrambler copies), a call the linker
removed, or a function this tree folded into a different helper.  Conversely
the object may inline where ours calls the same EXACT body; that direction is
reported as `extra` and is not reachable by a source spelling.  Read the
caller before treating a row as work, and keep the rule: a body-identical
spelling that moves the cost is a recovery; padding statements are not.

    make inlineedges                       # ranked screen
    make inlineedges S="--top 100"
    python3 tools/toolchain/inlineedges.py --self-test

The exact set comes from `byteident.py --list-exact`'s output so this tool
does not grow a second notion of the grade.  It refuses an empty or stale
object tree for the same reason every other aid does (F2400, F2401).
"""

import argparse
import collections
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import byteident                                                      # noqa: E402
import structsim                                                      # noqa: E402

Counter = collections.Counter


def edges(rows):
    """(direct call targets, tail-jump targets) as multisets."""
    calls, tails = Counter(), Counter()
    for row in rows:
        if row.startswith("call sym:"):
            calls[row[9:]] += 1
        elif row.startswith("jmp sym:"):
            tails[row[8:]] += 1
    return calls, tails


def crossing(blob_calls, ours_calls, blob_tails, ours_tails):
    """Return missing/extra calls and call->tail / tail->call conversions.

    A `call` on one side and a `tail` on the other is a sibling-call change,
    not an inlined body: it is counted separately so it cannot masquerade as
    the #333 class.
    """
    missing = blob_calls - ours_calls
    extra = ours_calls - blob_calls
    to_tail, to_call = Counter(), Counter()
    for target in list(missing):
        shared = min(missing[target], ours_tails[target])
        if shared:
            to_tail[target] = shared
            missing[target] -= shared
    for target in list(extra):
        shared = min(extra[target], blob_tails[target])
        if shared:
            to_call[target] = shared
            extra[target] -= shared
    missing = +missing
    extra = +extra
    return {
        "missing": missing,
        "extra": extra,
        "to_tail": to_tail,
        "to_call": to_call,
    }


def load_exact(path):
    """Names from byteident --list-exact ('  1234  symbol')."""
    names = set()
    try:
        fh = open(path)
    except OSError as exc:
        sys.exit("inlineedges.py: cannot read %s (%s).\n"
                 "  Generate it with: python3 tools/toolchain/byteident.py "
                 "--list-exact > %s" % (path, exc.strerror, path))
    with fh:
        started = False
        for line in fh:
            if line.strip() == "EXACT:":
                started = True
                continue
            if not started:
                continue
            parts = line.split()
            if len(parts) == 2 and parts[0].isdigit():
                names.add(parts[1])
    if not names:
        sys.exit("inlineedges.py: %s contains no EXACT names; a screen over "
                 "an empty exact set is not a result." % path)
    return names


def best_copy(sym, index):
    """Score every defining object; return the copy closest to the object.

    COMDAT bodies are duplicated across TUs.  The worst-copy rule belongs to
    the grade; for a SCREEN the closest copy is the one that shows the real
    edge difference instead of a copy chosen by TU layout.
    """
    bc, bt = edges(structsim.code(byteident.BLOB, sym)[0])
    best = None
    for obj in index[sym]:
        oc, ot = edges(structsim.code(obj, sym)[0])
        d = crossing(bc, oc, bt, ot)
        rank = (sum(d["missing"].values()) + sum(d["extra"].values()),
                sum(d["missing"].values()), obj)
        if best is None or rank < best[0]:
            best = (rank, d, obj, bc, oc)
    return best


def self_test():
    checks = []

    def check(label, cond):
        checks.append((label, bool(cond)))

    def e(rows):
        return edges(rows)

    b, bt = e(["mov R,R", "call sym:alpha", "call sym:beta", "ret "])
    o, ot = e(["mov R,R", "call sym:alpha", "call sym:beta", "ret "])
    d = crossing(b, o, bt, ot)
    check("identical edges -> nothing",
          not d["missing"] and not d["extra"])

    o, ot = e(["mov R,R", "call sym:beta", "ret "])
    d = crossing(b, o, bt, ot)
    check("one call absent -> missing alpha",
          d["missing"] == Counter({"alpha": 1}) and not d["extra"])

    o, ot = e(["call sym:beta", "mov R,R", "call sym:alpha", "ret "])
    d = crossing(b, o, bt, ot)
    check("reordered calls -> nothing",
          not d["missing"] and not d["extra"])

    o, ot = e(["call sym:alpha", "call sym:alpha", "call sym:beta"])
    d = crossing(b, o, bt, ot)
    check("duplicate counts -> multiplicity, not presence",
          d["extra"] == Counter({"alpha": 1}))

    o, ot = e(["call sym:alpha", "jmp sym:beta"])
    d = crossing(b, o, bt, ot)
    check("call -> sibling tail is not an inlined body",
          not d["missing"] and not d["extra"] and d["to_tail"] ==
          Counter({"beta": 1}))

    o, ot = e(["call sym:beta", "call sym:gamma"])
    d = crossing(b, o, bt, ot)
    check("different target is not a boundary change",
          d["missing"] == Counter({"alpha": 1}) and
          d["extra"] == Counter({"gamma": 1}))

    bc, bt = e(["call sym:x"])
    oc, ot = e(["call sym:x"])
    check("self-test has a negative control",
          not crossing(bc, oc, bt, ot)["missing"])

    bad = sum(not ok for _, ok in checks)
    for label, ok in checks:
        print("  %s  %s" % ("ok  " if ok else "FAIL", label))
    print("\n  %d self-test case(s), %d failure(s)" % (len(checks), bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--top", type=int, default=40, metavar="N")
    ap.add_argument("--exact-list", default="/tmp/inlineedges-exact.txt",
                    metavar="PATH",
                    help="output of byteident.py --list-exact")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()

    exact = load_exact(a.exact_list)
    index = structsim.require_objects()
    blob = structsim.blob_sizes()
    common = sorted(k for k in index if k in blob)
    if not common:
        sys.exit("inlineedges.py: no shared symbols; denominator is zero.")

    rows = []
    calls_blob = calls_ours = 0
    for sym in common:
        rank, d, obj, bc, oc = best_copy(sym, index)
        calls_blob += sum(bc.values())
        calls_ours += sum(oc.values())
        if not d["missing"] and not d["extra"]:
            continue
        missing_exact = sorted((t, n) for t, n in d["missing"].items()
                               if t in exact)
        extra_exact = sorted((t, n) for t, n in d["extra"].items() if t in exact)
        if not missing_exact and not extra_exact:
            continue
        size = blob.get(sym, 0)
        rows.append((len(missing_exact), sum(n for _, n in missing_exact),
                     len(extra_exact), size, sym, missing_exact, extra_exact,
                     len(index[sym])))

    rows.sort(key=lambda r: (-r[1], -r[0], -r[3], r[4]))
    print("  symbols compared: %d   call edges: blob %d, ours %d"
          % (len(common), calls_blob, calls_ours))
    print("  %d caller(s) with an EXACT-target edge difference\n" % len(rows))
    if not rows:
        print("  (none) -- no caller differs on a target that is itself "
              "byte-identical.")
        return 0
    print("  %-6s %-6s %-6s %5s  %s"
          % ("missT", "missN", "extraN", "blobB", "caller -> target"))
    for nm, nn, ne, size, sym, missing, extra, copies in rows[:a.top]:
        print("  %-6d %-6d %-6d %5d  %s"
              % (nm, nn, ne, size, sym[:58]))
        for target, n in missing:
            print("        missing x%d  %s" % (n, target[:70]))
        for target, n in extra:
            print("        extra   x%d  %s" % (n, target[:70]))
        if copies > 1:
            print("        (%d defining copies; closest scored)" % copies)
    return 0


if __name__ == "__main__":
    sys.exit(main())
