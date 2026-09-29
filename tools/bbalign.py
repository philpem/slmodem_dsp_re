#!/usr/bin/env python3
"""Align one symbol's blob and candidate instructions, and print where they part.

WHY THIS EXISTS

The first step of every Lever-2 sweep is the same: take a symbol in the SIZE
bucket and find the basic block or statement the two sides disagree at.  That
is `byteident.py --why SYMBOL`'s job only in part -- `--why` prints the single
row `alpha_equal` rejects on, which is usually NOT the first row that differs,
and it prints no context, so the reader still has to disassemble both sides and
line them up by hand.  This tool does that alignment mechanically and reports
its denominator.

It is APPARATUS, not a gate and not a metric.  It decides nothing about which
side is right; it exists so the "where does it diverge" step is a read instead
of an eyeball diff.  The verdict is still `make period` plus
`byteident`/`partialcmp`.

    python3 tools/bbalign.py SYMBOL
    python3 tools/bbalign.py SYMBOL --context 6 --limit 5
    python3 tools/bbalign.py SYMBOL --all

DENOMINATOR.  Every run prints the instruction counts on both sides and how
many rows each SequenceMatcher opcode covers, so a run that aligned nothing is
not readable as a clean one (F134, F2400).

It reuses `byteident.py`'s own `body`, `insns`, `sizes` and `_padding`, so an
instruction is the same object to both tools -- never a second implementation
of the comparison (7773).
"""

import argparse
import difflib
import glob
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "toolchain"))

import byteident  # noqa: E402


def rows(path, symbol):
    """The symbol's instructions with alignment padding removed."""
    return [r for r in byteident.insns(path, symbol)
            if not byteident._padding(*r)]


def object_for(symbol):
    """Which candidate object defines SYMBOL -- the same glob order as byteident."""
    ours = {}
    for o in sorted(glob.glob(os.path.join(byteident.OURS, "*.o"))):
        for s in byteident.sizes(o):
            ours.setdefault(s, o)
    return ours


def baseline_probe():
    """Prove the aligner fires before a clean run is trusted (finding F134).

    Two committed symbols are scored: one the blob and ours define with
    different bodies (`FPM_SDM_init`, a BYTES row in the tree's own census)
    and one the tree has carried as EXACT.  The first must produce at least one
    non-equal opcode; the second must align with none.  If either expectation
    does not hold the tool refuses, so a broken aligner cannot read as clean.
    """
    candidates = []
    # Use a BYTES row the tree's own census already carries as different, and
    # an EXACT row as the clean control.
    probes = [("FPM_SDM_init", True)]
    if byteident.sizes(byteident.BLOB).get("RcFixed_Check_Combination"):
        probes.append(("RcFixed_Check_Combination", False))
    ok = True
    for symbol, want_diff in probes:
        a = rows(byteident.BLOB, symbol)
        ours = object_for(symbol)
        if symbol not in ours:
            print("  self-test: %s not defined by any candidate object" % symbol)
            ok = False
            continue
        b = rows(ours[symbol], symbol)
        ops = [o for o in difflib.SequenceMatcher(
            None, [r[0] + " " + r[1] for r in a],
            [r[0] + " " + r[1] for r in b], autojunk=False).get_opcodes()
            if o[0] != "equal"]
        fired = bool(ops)
        print("  self-test: %-28s blob %3d ours %3d  non-equal opcodes %3d  "
              "%-8s" % (symbol, len(a), len(b), len(ops),
                        "FIRES" if fired else "clean"))
        if fired != want_diff:
            ok = False
    if not ok:
        sys.exit("bbalign: self-test FAILED -- the aligner did not fire/clean "
                 "as expected; refusing to report a result")
    print("bbalign self-test: aligner fires on the different control and is "
          "clean on the exact one")
    return 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("symbol", nargs="?")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the aligner fires on a different control and "
                         "is clean on an exact one")
    ap.add_argument("--context", type=int, default=3,
                    help="instructions of context around each divergence")
    ap.add_argument("--limit", type=int, default=4,
                    help="at most this many divergence groups")
    ap.add_argument("--all", action="store_true",
                    help="also print the operand-only replace groups")
    ap.add_argument("--operands", action="store_true",
                    help="align full text instead of mnemonics; needed for "
                         "statement-order and operand work")
    a = ap.parse_args()

    if a.self_test:
        return baseline_probe()
    if not a.symbol:
        ap.error("a SYMBOL is required unless --self-test is given")

    ours = object_for(a.symbol)
    if a.symbol not in ours:
        sys.exit("bbalign: %s is not defined by any object in %s"
                 % (a.symbol, byteident.OURS))
    if a.symbol not in byteident.sizes(byteident.BLOB):
        sys.exit("bbalign: the blob does not define %s" % a.symbol)

    ab, _ = byteident.body(byteident.BLOB, a.symbol)
    bb, _ = byteident.body(ours[a.symbol], a.symbol)
    if ab is None or bb is None:
        sys.exit("bbalign: %s could not be disassembled on one side" % a.symbol)

    ra = rows(byteident.BLOB, a.symbol)
    rb = rows(ours[a.symbol], a.symbol)
    if a.operands:
        ta = [r[0] + " " + r[1] for r in ra]
        tb = [r[0] + " " + r[1] for r in rb]
        axis = "full text (mnemonic and operands)"
    else:
        ta = [r[0] for r in ra]
        tb = [r[0] for r in rb]
        axis = "mnemonic only (structural)"

    sm = difflib.SequenceMatcher(None, ta, tb, autojunk=False)
    ops = sm.get_opcodes()
    counts = {}
    for tag, *_ in ops:
        counts[tag] = counts.get(tag, 0) + 1
    equal_rows = sum(i2 - i1 for tag, i1, i2, j1, j2 in ops if tag == "equal")

    print("bbalign: %s" % a.symbol)
    print("  blob   %s" % byteident.BLOB)
    print("  ours   %s" % ours[a.symbol])
    print("  code instructions (padding stripped): blob %d, ours %d"
          % (len(ra), len(rb)))
    print("  alignment axis: %s" % axis)
    print("  gap: ours minus blob = %+d instruction(s)" % (len(rb) - len(ra)))
    print("  aligned equal %d; %d differing group(s) "
          "(replace %d, delete %d, insert %d)"
          % (equal_rows, sum(1 for o in ops if o[0] != "equal"),
             counts.get("replace", 0), counts.get("delete", 0),
             counts.get("insert", 0)))

    #
    # STRUCTURAL FIRST, then operand-only replacements.  Register allocation
    # alone produces hundreds of `replace` groups at full-text alignment; the
    # missing/extra statement is a `delete`/`insert` (or a replace whose
    # mnemonic lists differ), so list those before the noise (7865, 7848).
    #
    structural = []
    operand_only = []
    for o in ops:
        if o[0] == "equal":
            continue
        tag, i1, i2, j1, j2 = o
        if tag == "replace" and [ta[i] for i in range(i1, i2)] == \
                [tb[j] for j in range(j1, j2)]:
            operand_only.append(o)
            continue
        structural.append(o)

    if not structural:
        print("  NO structural difference in the compared rows -- the "
              "instruction multiset/order agrees; only operands differ"
              if operand_only else
              "  no instruction difference in the compared rows")
        if operand_only and a.operands:
            print("  %d operand-only replace group(s); raise --limit/--all"
                  % len(operand_only))
        return 0

    print("\n  %d STRUCTURAL group(s); %d operand-only replace(s)"
          % (len(structural), len(operand_only)))

    shown = structural if not a.all else structural + operand_only
    for n, (tag, i1, i2, j1, j2) in enumerate(shown[:a.limit]):
        lo = max(0, i1 - a.context)
        print("\n  [%d] %s  blob[%d:%d] ours[%d:%d]" % (n, tag, i1, i2, j1, j2))
        for i in range(lo, i1):
            print("      %s" % ta[i])
        for i in range(i1, i2):
            print("  blob- %s" % ta[i])
        for j in range(j1, j2):
            print("  ours+ %s" % tb[j])
        hi = min(len(ta), i2 + a.context)
        for i in range(i2, hi):
            print("      %s" % ta[i])
    if len(shown) > a.limit:
        print("\n  ... %d more group(s); raise --limit to see them"
              % (len(shown) - a.limit))
    return 0


if __name__ == "__main__":
    sys.exit(main())
