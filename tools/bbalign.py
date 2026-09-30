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

ANCHOR MODE (`--anchors`) is the mode for a symbol whose one side inlines
everything the other keeps out-of-line.  The mnemonic aligner above reads `mov`
against `mov` everywhere and aligns 624 rows of `v34handshak` on nothing; this
mode aligns only on CONTENT that survives scheduling and register allocation --
a named GLOBAL a relocation calls, the string a `.rodata.str` load addresses, a
named data object, or an indirect jump through a `.rodata` table -- and reports
the basic-block regions BETWEEN matched anchors that do not correspond.  The
anchors still inside an unmatched region are what name the SOURCE FAMILY it
belongs to, and a call-anchor census (blob/ours) is printed beside them.

    python3 tools/bbalign.py SYMBOL --anchors
    python3 tools/bbalign.py SYMBOL --anchors --ours PATH/TO/obj.o
    python3 tools/bbalign.py SYMBOL --anchors --min-insns 40 --focus bitreverse
    python3 tools/bbalign.py --anchor-self-test        # prove it fires

DENOMINATOR.  Every run prints the instruction counts on both sides and how
many rows each SequenceMatcher opcode covers, so a run that aligned nothing is
not readable as a clean one (F134, F2400).  Anchor mode prints the anchor
counts, the matched count and its rate on both sides, the instruction coverage,
and the region census on every run; `--anchor-self-test` prints the denominator
of each control and REFUSES to pass if the identity control is not clean, if a
register-only difference is reported as unmatched, or if the cross-object run
has a zero denominator (F134, F2401).  A NEGATIVE control is included for the
reason the tool exists: `FPM_SDM_init` differs only by a register renaming, and
a classifier that called that an unmatched region would flood every real run.

It reuses `byteident.py`'s own `body`, `insns`, `sizes` and `_padding`, so an
instruction is the same object to both tools -- never a second implementation
of the comparison (7773).
"""

import argparse
import difflib
import glob
import os
import re
import subprocess
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


# ---------------------------------------------------------------------------
# ANCHOR ALIGNMENT
#
# The mnemonic aligner above is defeated by the most common shape in this
# object: a function whose one side inlines everything the other keeps
# out-of-line.  `v34handshak` is 12,199 blob instructions against 1,378 when
# the profile is retained, and SequenceMatcher reads `mov` against `mov`
# everywhere, aligning 624 rows on nothing at all.
#
# An ANCHOR is content that survives scheduling and register allocation: the
# named GLOBAL a relocation calls, the string a `.rodata.str` load addresses,
# a named data object, or an indirect jump through a `.rodata` table.  Two
# streams aligned only on anchors part exactly where a whole basic block was
# added, removed or restructured -- and the anchors still inside an unmatched
# region are what name the SOURCE FAMILY it belongs to.
#
# This is APPARATUS.  It decides nothing about which side is right; it exists
# so "which family do I test next" is a read.  The verdict is still
# `make period` plus `byteident`.
# ---------------------------------------------------------------------------

RAW_INSN = re.compile(r"^\s*([0-9a-f]+):\t([0-9a-f ]*)\t(.*)$")


def stream(path, symbol):
    """[(dict)] with canonical relocations attached to their instruction.

    Reuses `byteident.body` for the canonical target of every relocation, so
    an anchor is the same object to this tool and to the byte-identity checker
    (7773) -- never a second implementation of the comparison.
    """
    raw, relocs = byteident.body(path, symbol)
    if raw is None:
        return None, None
    text = subprocess.run(
        ["objdump", "-dr", "--disassemble=" + symbol, path],
        capture_output=True, text=True).stdout
    rows, base = [], None
    for line in text.splitlines():
        m = RAW_INSN.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        if base is None:
            base = addr
        nbytes = len(m.group(2).split())
        instruction = m.group(3).split("#", 1)[0]
        instruction = re.sub(r"<[^>]*>", "", instruction).strip()
        parts = instruction.split(None, 1)
        mn = parts[0]
        ops = parts[1].strip() if len(parts) > 1 else ""
        rows.append({"addr": addr, "size": nbytes, "mn": mn, "ops": ops,
                     "rel": []})
    for off, (kind, tgt) in relocs.items():
        a = base + off
        for r in rows:
            if r["addr"] <= a < r["addr"] + r["size"]:
                r["rel"].append((kind, tgt))
                break
    return base, rows


def anchor_of(r):
    """The content anchor this instruction carries, or None.

    Only cross-object STABLE content is an anchor.  A named GLOBAL (call or
    data reference) resolves to its name on both sides.  A string load
    resolves to the string bytes through the merge-section addend.  A jump
    table is keyed generically: its `.rodata` addend is a layout artefact and
    its entries are addresses, neither of which is stable across objects.
    """
    for kind, tgt in r["rel"]:
        if kind == "R_386_PC32" and tgt[0] == "symbol":
            return ("call", tgt[1], tgt[2])
        if kind == "R_386_32":
            if tgt[0] == "merge-string":
                return ("str", tgt[1])
            if tgt[0] == "symbol":
                return ("data", tgt[1], tgt[2])
            if tgt[0] == "section" and r["mn"].startswith("jmp"):
                return ("jtab",)
    return None


def _anchors(rows):
    return [(i, anchor_of(r)) for i, r in enumerate(rows)
            if anchor_of(r) is not None]


def _align(a_rows, b_rows):
    """Longest consistent anchor subsequence as ordered (ia, ib) pairs."""
    A, B = _anchors(a_rows), _anchors(b_rows)
    sm = difflib.SequenceMatcher(
        None, [k for _, k in A], [k for _, k in B], autojunk=False)
    pairs = []
    for ai, bi, n in sm.get_matching_blocks():
        for t in range(n):
            pairs.append((A[ai + t][0], B[bi + t][0]))
    return A, B, pairs


def _code_text(rows):
    return [(r["mn"], r["ops"]) for r in rows
            if not byteident._padding(r["mn"], r["ops"])]


def _regions(a_rows, b_rows, pairs):
    out, pa, pb = [], -1, -1
    for ia, ib in pairs:
        out.append((pa + 1, ia, pb + 1, ib))
        pa, pb = ia, ib
    out.append((pa + 1, len(a_rows), pb + 1, len(b_rows)))
    return out


def _classify(a_slice, b_slice):
    a, b = _code_text(a_slice), _code_text(b_slice)
    if not a and not b:
        return None
    if not a:
        return "OURS-ONLY"
    if not b:
        return "BLOB-ONLY"
    if [x[0] for x in a] == [x[0] for x in b]:
        return None if a == b else "SAME-SHAPE"
    return "DIFFERENT"


UNMATCHED = ("BLOB-ONLY", "OURS-ONLY", "DIFFERENT")


def _fmt_anchor(k):
    if k[0] == "call":
        return "call %s" % k[1]
    if k[0] == "str":
        s = k[1].rstrip(b"\0").decode("latin1")
        s = (s.replace("\\", "\\\\").replace("\n", "\\n")
             .replace("\"", "\\\""))
        return 'str "%s"' % s[:44]
    if k[0] == "data":
        return "data %s+%d" % (k[1], k[2])
    if k[0] == "jtab":
        return "jump-table"
    return str(k)


def _region_anchors(rows, lo, hi):
    seen, out = set(), []
    for r in rows[lo:hi]:
        k = anchor_of(r)
        if k is not None and k not in seen:
            seen.add(k)
            out.append(_fmt_anchor(k))
    return out


def _addr_span(rows, lo, hi):
    if lo >= hi:
        return "-"
    return "0x%x..0x%x" % (rows[lo]["addr"], rows[hi - 1]["addr"])


def _show(rows, lo, hi, insns):
    for r in rows[lo:min(hi, lo + insns)]:
        print("      %-10s %s %s" % ("0x%x:" % r["addr"], r["mn"], r["ops"]))
    if hi - lo > insns:
        print("      ... %d more instruction(s)" % (hi - lo - insns))


def anchor_report(symbol, ours_path, limit, insns, min_insns=1, focus=None):
    base_a, a = stream(byteident.BLOB, symbol)
    base_b, b = stream(ours_path, symbol)
    if a is None or b is None:
        sys.exit("bbalign --anchors: %s could not be disassembled on one side"
                 % symbol)

    A, B, pairs = _align(a, b)
    regs = _regions(a, b, pairs)

    counts, un, cov_a, cov_b = {}, [], 0, 0
    for (al, ah, bl, bh) in regs:
        tag = _classify(a[al:ah], b[bl:bh])
        counts[tag] = counts.get(tag, 0) + 1
        if tag in UNMATCHED:
            un.append((tag, al, ah, bl, bh))
        else:
            cov_a += (ah - al)
            cov_b += (bh - bl)
    cov_a += len(pairs)
    cov_b += len(pairs)

    matched = len(pairs)
    print("bbalign --anchors: %s" % symbol)
    print("  blob   %s  (%d instructions)" % (byteident.BLOB, len(a)))
    print("  ours   %s  (%d instructions)" % (ours_path, len(b)))
    print("  anchors: blob %d, ours %d; MATCHED %d (%.1f%% of blob, %.1f%% "
          "of ours)  (denominator: anchors)"
          % (len(A), len(B), matched,
             100.0 * matched / max(1, len(A)), 100.0 * matched / max(1, len(B))))
    print("  coverage: blob %d/%d instructions (%.1f%%), ours %d/%d (%.1f%%)"
          % (cov_a, len(a), 100.0 * cov_a / max(1, len(a)),
             cov_b, len(b), 100.0 * cov_b / max(1, len(b))))
    print("  regions: %d  (identical %d, same-shape %d, blob-only %d, "
          "ours-only %d, different %d)"
          % (len(regs), counts.get(None, 0), counts.get("SAME-SHAPE", 0),
             counts.get("BLOB-ONLY", 0), counts.get("OURS-ONLY", 0),
             counts.get("DIFFERENT", 0)))
    if not A and not B:
        print("  NOTE: no anchor on either side; the whole function is one "
              "region and only its text was compared")
    print("  unmatched regions: %d" % len(un))

    #
    # THE CALL-BOUNDARY DELTA IS THE F11520/F11522 CENSUS, computed from the
    # same anchors the alignment used.  It is content: a target whose count
    # differs is a source-expression or helper-boundary difference, and it is
    # the second half of "which family do I test next".
    #
    cb, co = _call_counts(a), _call_counts(b)
    delta = sorted((abs(co.get(t, 0) - cb.get(t, 0)), t)
                   for t in set(cb) | set(co)
                   if cb.get(t, 0) != co.get(t, 0))
    if delta:
        print("  call-anchor deltas (blob/ours): %s"
              % ", ".join("%s %d/%d" % (t, cb.get(t, 0), co.get(t, 0))
                          for _, t in delta[:12]))
        if len(delta) > 12:
            print("    ... %d more" % (len(delta) - 12))
    else:
        print("  call-anchor deltas: none")

    #
    # RANK THE MAP BY SIZE.  A two-instruction region is register churn; the
    # one that names a source family is the large block that is all there on
    # one side and absent on the other.  `--focus NAME` keeps only regions
    # that touch one call, which is how the bitreverse lead is read straight
    # off the map.
    #
    def _anchor_names(rows, lo, hi):
        names = set()
        for r in rows[lo:hi]:
            k = anchor_of(r)
            if k is not None and k[0] == "call":
                names.add(k[1])
        return names

    kept = []
    for (tag, al, ah, bl, bh) in un:
        if max(ah - al, bh - bl) < min_insns:
            continue
        if focus:
            names = _anchor_names(a, al, ah) | _anchor_names(b, bl, bh)
            if focus not in names:
                continue
        kept.append((tag, al, ah, bl, bh))
    kept.sort(key=lambda r: -max(r[2] - r[1], r[4] - r[3]))
    print("  shown regions: %d (min %d insn(s)%s)"
          % (len(kept), min_insns, ", focus %s" % focus if focus else ""))

    for n, (tag, al, ah, bl, bh) in enumerate(kept[:limit]):
        print("\n  [%d] %s  blob %s (%d insns)  ours %s (%d insns)"
              % (n, tag, _addr_span(a, al, ah), ah - al,
                 _addr_span(b, bl, bh), bh - bl))
        ka = _region_anchors(a, al, ah)
        kb = _region_anchors(b, bl, bh)
        if ka:
            print("      blob anchors here: %s" % "; ".join(ka[:8]))
            if len(ka) > 8:
                print("        ... %d more" % (len(ka) - 8))
        if kb:
            print("      ours anchors here: %s" % "; ".join(kb[:8]))
            if len(kb) > 8:
                print("        ... %d more" % (len(kb) - 8))
        if ah > al:
            print("      blob:")
            _show(a, al, ah, insns)
        if bh > bl:
            print("      ours:")
            _show(b, bl, bh, insns)
    if len(kept) > limit:
        print("\n  ... %d more shown region(s); raise --limit"
              % (len(kept) - limit))
    return 0


def _call_counts(rows):
    c = {}
    for r in rows:
        for kind, tgt in r["rel"]:
            if kind == "R_386_PC32" and tgt[0] == "symbol":
                c[tgt[1]] = c.get(tgt[1], 0) + 1
    return c


def anchor_self_test(ours_path=None):
    """Prove the anchor aligner fires, and print every denominator (F134)."""
    ok = True

    # (1) A stream against ITSELF must have zero unmatched regions.  This is
    # the exact-symbol control and it cannot be fooled by the object being
    # large: identity alignment is a single matching block.
    _, a = stream(byteident.BLOB, "v34handshak")
    A, _, pairs = _align(a, a)
    bad = [r for r in _regions(a, a, pairs)
           if _classify(a[r[0]:r[1]], a[r[2]:r[3]]) is not None]
    print("  self-test: v34handshak vs itself: matched %d of %d anchors, "
          "%d unmatched region(s)" % (len(pairs), len(A), len(bad)))
    if bad or len(pairs) != len(A) or not A:
        ok = False

    # (2) The NEGATIVE control that decides whether this tool is usable:
    # `FPM_SDM_init` differs from the tree only by a register renaming.  A
    # classifier that called that an unmatched region would flood every real
    # run, so it must report ZERO unmatched and at least one SAME-SHAPE.
    diff_sym = "FPM_SDM_init"
    ours = object_for(diff_sym)
    if diff_sym in ours:
        _, da = stream(byteident.BLOB, diff_sym)
        _, db = stream(ours[diff_sym], diff_sym)
        _, _, dp = _align(da, db)
        dcls = [_classify(da[r[0]:r[1]], db[r[2]:r[3]])
                for r in _regions(da, db, dp)]
        dun = [c for c in dcls if c in UNMATCHED]
        print("  self-test: %s register-only control: %d/%d instruction(s) "
              "each side, %d SAME-SHAPE, %d unmatched region(s)"
              % (diff_sym, len(da), len(db),
                 sum(1 for c in dcls if c == "SAME-SHAPE"), len(dun)))
        if dun or "SAME-SHAPE" not in dcls:
            ok = False
    else:
        print("  self-test: %s not defined by a candidate object" % diff_sym)
        ok = False

    # (3) The real cross-object run: a nonzero anchor denominator and at
    # least one unmatched region.  A dead aligner would read 0 and 0.
    target = ours_path or object_for("v34handshak")["v34handshak"]
    _, ta = stream(byteident.BLOB, "v34handshak")
    _, tb = stream(target, "v34handshak")
    A2, B2, p2 = _align(ta, tb)
    n2 = [r for r in _regions(ta, tb, p2)
          if _classify(ta[r[0]:r[1]], tb[r[2]:r[3]]) in UNMATCHED]
    print("  self-test: v34handshak vs %s: anchors blob %d ours %d, matched "
          "%d; blob %d vs ours %d instructions; %d unmatched region(s)"
          % (os.path.basename(target), len(A2), len(B2), len(p2),
             len(ta), len(tb), len(n2)))
    if not p2 or not n2:
        ok = False

    # (4) The F11522 lead, when an aggressive-profile object is named: the
    # blob calls `bitreverse` six times in this function and the candidate
    # eight.  That is a hard check here, so a run that names the object
    # cannot read clean if the tool has stopped seeing call anchors.
    if ours_path:
        cb, co = _call_counts(ta), _call_counts(tb)
        print("  self-test: call-anchor census bitreverse: blob %d, ours %d"
              % (cb.get("bitreverse", 0), co.get("bitreverse", 0)))
        if cb.get("bitreverse", 0) >= co.get("bitreverse", 0):
            ok = False

    if not ok:
        sys.exit("bbalign --anchor-self-test FAILED: an expected fire or clean "
                 "did not happen; refusing to report a result")
    print("bbalign anchor self-test: identity is clean, the register-only "
          "control does not over-report, and the cross-object run has a "
          "nonzero denominator")
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
    ap.add_argument("--anchors", action="store_true",
                    help="align by content anchors (global calls, strings, "
                         "jump tables) and report the unmatched basic blocks")
    ap.add_argument("--anchor-self-test", action="store_true",
                    help="prove the anchor aligner fires and is clean, "
                         "printing every denominator")
    ap.add_argument("--ours", metavar="OBJECT",
                    help="candidate object to compare instead of the one the "
                         "tree's build directory defines")
    ap.add_argument("--insns", type=int, default=10,
                    help="instructions of each unmatched region to disassemble")
    ap.add_argument("--min-insns", type=int, default=1,
                    help="only report unmatched regions at least this large")
    ap.add_argument("--focus", metavar="TARGET",
                    help="only report regions whose anchors touch this call "
                         "target, e.g. bitreverse")
    a = ap.parse_args()

    if a.anchor_self_test:
        return anchor_self_test(a.ours)
    if a.self_test:
        return baseline_probe()
    if not a.symbol:
        ap.error("a SYMBOL is required unless --self-test is given")

    if a.anchors:
        if a.symbol not in byteident.sizes(byteident.BLOB):
            sys.exit("bbalign: the blob does not define %s" % a.symbol)
        path = a.ours or object_for(a.symbol).get(a.symbol)
        if not path:
            sys.exit("bbalign: %s is not defined by any object in %s"
                     % (a.symbol, byteident.OURS))
        return anchor_report(a.symbol, path, a.limit, a.insns,
                             a.min_insns, a.focus)

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
