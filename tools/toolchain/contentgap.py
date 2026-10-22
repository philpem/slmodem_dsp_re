#!/usr/bin/env python3
"""Screen instruction surpluses without claiming source equivalence.

THE QUESTION THIS ANSWERS

`byteident.py` says a symbol is SIZE and how many bytes differ.  That is not
enough to decide what to inspect next. Missing operations, different source
control flow, value capture, types and scheduling can all produce a SIZE
result. This screen reports projections that help select an investigation;
none of its counts establishes source equivalence or closes reconstruction.

An INSTRUCTION MULTISET detects multiplicity differences hidden by alignment.
Equality of that multiset says only that each projected instruction has a
counterpart. Dependent operation order, register relationships and branch
conditions can differ while the projection agrees. Conversely, a surplus can
come from compiler instruction selection rather than a missing operation.

WHAT IS REPORTED

    blob N insns, ours M        the denominator, always printed
    blob-only / ours-only       how many instructions the alignment removed,
                                in how many hunks, and the largest hunk
    unique to blob / ours       the multiset surplus: instructions present on
                                one side more times than on the other
    shape                       a statement of those two numbers, not a grade

Per-hunk attribution: the global surplus is charged to the first hunks that
carry those instructions, so `unique` on a hunk means "this instruction has no
counterpart anywhere in the other build", not "this hunk is wholly novel".

Canonicalisation is `structsim.py`'s, imported rather than re-grown: registers
become `R`, branch opcodes become classes, a call keeps WHICH symbol it targets
and loses the displacement,
section-relative `.rodata`/`.bss` addends are masked (they are per-TU layout
artefacts), and small displacements and immediates are KEPT, because they
carry the structure.  Alignment padding (`nop`, self-moves, `lea 0x0(%reg,
%reg,1),%reg`) is stripped exactly as `byteident.py --near` strips it.

WHAT IT CANNOT COMPARE

A symbol whose arms the object INLINED into one function is not comparable: the
object's `v34handshak` holds every microstate body (12,199 instructions) where
our tree emits the dispatcher alone (3,059), so it heads any `--worst` ranking
by construction.  Compare the per-arm symbols instead; the same caveat applies
to any function the two builds factored differently.

WHAT THIS IS NOT

Not a grade and not a gate.  A `unique` count of a few instructions can still
be the compiler choosing a different constant materialisation, and a zero
merely says no projected instruction is absent. It does not establish control
flow, operand ownership, read timing or source fidelity.
Use it to decide whether to READ the function at all, and record the numbers
beside the finding: "SIZE (48 bytes)" alone cannot tell the two cases apart.

    make contentgap S=modulatevector
    python3 tools/toolchain/contentgap.py demapFrame initdigital
    python3 tools/toolchain/contentgap.py --worst 40

The tool refuses to run on an empty or stale object tree (findings F2400,
F2401); a census over zero symbols renders as a clean result.
"""

import argparse
import collections
import difflib
import multiprocessing
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

# One parser, one notion of padding, one canonicalisation: these belong to
# byteident.py and structsim.py, and this tool must not grow a second opinion
# about any of them.
import byteident                                                      # noqa: E402
import structsim                                                      # noqa: E402

code = structsim.code            # canonical list, padding stripped
canon = structsim.canon          # one instruction, registers masked

#
# An OPERAND-masked projection. Different stack slots or arithmetic constants
# can agree here without representing the same value or statement. The strict
# count retains those operands; neither count reconstructs register dataflow
# or CFG edges. structsim.canon also folds branch opcodes into branch classes.
#
OPERAND = re.compile(r"\$?-?0x[0-9a-f]+|\$\d+")


def relaxed(rows):
    """Operand-masked instruction list."""
    return [OPERAND.sub("N", x) for x in rows]


def mnemonics(rows):
    """Instruction KINDS only: the weakest test, least confounded by codegen."""
    return [x.split()[0] for x in rows]


def surplus(blob, ours):
    """Instructions present more times on one side than the other."""
    cb, co = collections.Counter(blob), collections.Counter(ours)
    return cb - co, co - cb


def split(blob, ours):
    """Alignment hunks and the multiset verdict for one symbol."""
    sb, so = surplus(blob, ours)
    sm = difflib.SequenceMatcher(a=blob, b=ours, autojunk=False)
    deleted, inserted = [], []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag in ("replace", "delete"):
            deleted.append(blob[i1:i2])
        if tag in ("replace", "insert"):
            inserted.append(ours[j1:j2])
    #
    # Charge the GLOBAL surplus to the hunks that carry those instructions.
    # A hunk's `unique` therefore means "no counterpart anywhere in the other
    # build"; an instruction that merely moved is not charged to either side.
    left = collections.Counter(sb)
    del_unique = []
    for hunk in deleted:
        take = collections.Counter()
        for ins in hunk:
            if left[ins] > 0:
                take[ins] += 1
                left[ins] -= 1
        del_unique.append(sum(take.values()))
    left = collections.Counter(so)
    ins_unique = []
    for hunk in inserted:
        take = collections.Counter()
        for ins in hunk:
            if left[ins] > 0:
                take[ins] += 1
                left[ins] -= 1
        ins_unique.append(sum(take.values()))
    return {
        "blob": len(blob),
        "ours": len(ours),
        "deleted": deleted,
        "inserted": inserted,
        "del_unique": del_unique,
        "ins_unique": ins_unique,
        "unique_blob": sum(sb.values()),
        "unique_ours": sum(so.values()),
        "surplus_blob": sorted(sb.items(), key=lambda kv: (-kv[1], kv[0])),
        "surplus_ours": sorted(so.items(), key=lambda kv: (-kv[1], kv[0])),
    }


def shape(d):
    """A statement of the multiset result, in the words the playbook uses."""
    if d["unique_blob"] == 0 and d["unique_ours"] == 0:
        return ("zero surplus in the operand-masked projection; control flow, "
                "register relationships and operand values remain unchecked")
    if d["mn_blob"] == 0 and d["mn_ours"] == 0:
        return ("same projected instruction KINDS, different operand shapes; "
                "inspect operands and dataflow before classifying the cause")
    return ("%d instruction kind(s) have no counterpart -- READ the surplus "
            "list: a different MNEMONIC is often still the compiler's choice "
            "(lea for add, xor for mov $0), so this is a question, not a "
            "verdict" % max(d["mn_blob"], d["mn_ours"]))


def measure(sym, index, verbose=True):
    """Worst COMDAT copy wins, as byteident and structsim both do."""
    blob = structsim.blob_sizes()
    if sym not in blob:
        sys.exit("contentgap.py: the blob does not define %s" % sym)
    if sym not in index:
        sys.exit("contentgap.py: no object in %s defines %s"
                 % (byteident.OURS, sym))
    blob_code, padded_b = code(byteident.BLOB, sym)
    blob_rel = relaxed(blob_code)
    mnemonic_blob = mnemonics(blob_code)
    best = None
    for obj in index[sym]:
        ours_code, padded_o = code(obj, sym)
        d = split(blob_rel, relaxed(ours_code))
        s = split(blob_code, ours_code)
        m = split(mnemonic_blob, mnemonics(ours_code))
        d["strict_blob"], d["strict_ours"] = s["unique_blob"], s["unique_ours"]
        d["mn_blob"], d["mn_ours"] = m["unique_blob"], m["unique_ours"]
        #
        # The most CONTENT we could be missing decides which copy to show when
        # a symbol is COMDAT-duplicated; ties go to the longer ours.
        key = (-d["unique_blob"], -d["unique_ours"], -d["ours"])
        if best is None or key < best[0]:
            best = (key, d, obj, padded_o)
    _, d, obj, padded_o = best
    d["obj"], d["padded_blob"], d["padded_ours"] = obj, padded_b, padded_o
    if not verbose:
        return d
    print("%s" % sym)
    print("  blob %d insns, ours %d insns   (%d/%d alignment rows stripped)"
          % (d["blob"], d["ours"], padded_b, padded_o))
    print("  blob-only : %d insn(s) in %d hunk(s), largest %d"
          % (sum(len(h) for h in d["deleted"]), len(d["deleted"]),
             max([len(h) for h in d["deleted"]] or [0])))
    print("  ours-only : %d insn(s) in %d hunk(s), largest %d"
          % (sum(len(h) for h in d["inserted"]), len(d["inserted"]),
             max([len(h) for h in d["inserted"]] or [0])))
    print("  kinds     : unique %d blob / %d ours   (mnemonic only)"
          % (d["mn_blob"], d["mn_ours"]))
    print("  structure : unique %d blob / %d ours   (operands masked)"
          % (d["unique_blob"], d["unique_ours"]))
    print("  operands  : unique %d blob / %d ours   (operands kept)"
          % (d["strict_blob"], d["strict_ours"]))
    print("  reading   : %s" % shape(d))
    print("  object    : %s" % obj)
    if len(index[sym]) > 1:
        print("  %d defining object(s); the copy with the most unique blob "
              "instructions scored" % len(index[sym]))
    for label, rows in (("blob", d["surplus_blob"]), ("ours", d["surplus_ours"])):
        if not rows:
            continue
        print("  surplus only in %s (top 10 of %d):" % (label, len(rows)))
        for ins, n in rows[:10]:
            print("     %-46s x%d" % (ins, n))
    return d


_RANK = {}


def _rank_one(sym):
    """Worker: unique-to-blob first, then unique-to-ours.  Forked child."""
    index = _RANK["index"]
    try:
        raw = code(byteident.BLOB, sym)[0]
    except Exception:
        return ("skip", sym, "unreadable")
    best = None
    for obj in index[sym]:
        try:
            ours_raw = code(obj, sym)[0]
        except Exception:
            return ("skip", sym, "unreadable")
        m = split(mnemonics(raw), mnemonics(ours_raw))
        d = split(relaxed(raw), relaxed(ours_raw))
        key = (-m["unique_blob"], -m["unique_ours"],
               -d["unique_blob"], -d["unique_ours"])
        if best is None or key < best[0]:
            best = (key, m, d, len(raw), len(ours_raw))
    _, m, d, nb, no = best
    return (m["unique_blob"], m["unique_ours"],
            d["unique_blob"], d["unique_ours"], nb, no, sym)


def worst(index, count):
    """Rank by instructions with no counterpart anywhere on the other side."""
    blob = structsim.blob_sizes()
    common = sorted(k for k in index if k in blob)
    if not common:
        sys.exit("contentgap.py: blob and %s share NO symbols; the denominator "
                 "is zero, which is not a result." % byteident.OURS)
    _RANK["index"] = index
    jobs = max(1, min(os.cpu_count() or 1, 8))
    rows, skipped = [], []
    try:
        with multiprocessing.Pool(jobs) as pool:
            for item in pool.imap_unordered(_rank_one, common, chunksize=8):
                if item is None:
                    continue
                if item[0] == "skip":
                    skipped.append((item[1], item[2]))
                else:
                    rows.append(item)
    except (OSError, ValueError):
        for sym in common:
            item = _rank_one(sym)
            if item and item[0] == "skip":
                skipped.append((item[1], item[2]))
            elif item:
                rows.append(item)
    rows.sort(key=lambda r: (-r[0], -r[1], -r[2]))
    print("  %d of %d shared symbol(s) scored; %d skipped"
          % (len(rows), len(common), len(skipped)))
    print("  %-6s %-6s %-6s %-6s %5s %5s  %s"
          % ("kindB", "kindO", "strB", "strO", "blob", "ours", "symbol"))
    for kb, ko, sb, so, nb, no, sym in rows[:count]:
        print("  %-6d %-6d %-6d %-6d %5d %5d  %s"
              % (kb, ko, sb, so, nb, no, sym[:56]))
    if skipped:
        kinds = {}
        for _, why in skipped:
            kinds[why] = kinds.get(why, 0) + 1
        print("  skipped: " + ", ".join("%d %s" % (v, k)
                                        for k, v in sorted(kinds.items())))
    return 0


def self_test():
    """Check surplus accounting and demonstrate its known blind spots."""
    checks = []

    def check(label, cond):
        checks.append((label, bool(cond)))

    a = ["mov R,R", "add R,R", "cmp R,R", "jle A", "ret "]
    d = split(a, a)
    check("identical -> nothing unique, no hunks",
          d["unique_blob"] == 0 and d["unique_ours"] == 0
          and not d["deleted"] and not d["inserted"])

    # The whole point: a pure reordering is a deletion in alignment terms but
    # has no unique instruction on either side.
    reordered = ["cmp R,R", "jle A", "mov R,R", "add R,R", "ret "]
    d = split(a, reordered)
    check("reordering -> hunks exist, nothing unique",
          sum(len(h) for h in d["deleted"]) > 0
          and d["unique_blob"] == 0 and d["unique_ours"] == 0)

    # A genuinely absent instruction.
    d = split(a[:3] + ["xor R,R"] + a[3:], a)
    check("blob-only instruction -> unique to blob",
          d["unique_blob"] == 1 and d["unique_ours"] == 0)

    d = split(a, a[:3] + ["xor R,R"] + a[3:])
    check("ours-only instruction -> unique to ours",
          d["unique_ours"] == 1 and d["unique_blob"] == 0)

    # Multiplicity matters: two extra copies of the SAME instruction are two
    # missing instructions, however the alignment happens to place them.
    d = split(a + ["inc R", "inc R"], a + ["inc R"])
    check("duplicate counts as surplus, not a reordering",
          d["unique_blob"] == 1)

    # `split()` takes CANONICAL lists, so these two go through canon() the way
    # the real path does.  Registers are masked by canon, constants are not.
    d = split([canon("mov", "0x14(%ebp),%eax")],
              [canon("mov", "0x14(%esp),%ebx")])
    check("register rename -> nothing unique",
          d["unique_blob"] == 0 and d["unique_ours"] == 0)
    d = split([canon("cmp", "$0x7f,%ax")], [canon("cmp", "$0x10,%ax")])
    check("different constant is unique with operands KEPT",
          d["unique_blob"] == 1 and d["unique_ours"] == 1)
    d = split(relaxed([canon("cmp", "$0x7f,%ax")]),
              relaxed([canon("cmp", "$0x10,%ax")]))
    check("different constants agree in the MASKED projection",
          d["unique_blob"] == 0 and d["unique_ours"] == 0)
    d = split(relaxed([canon("mov", "0x34(%esp),%eax")]),
              relaxed([canon("mov", "0x38(%esp),%ebx")]))
    check("different displacements agree in the MASKED projection",
          d["unique_blob"] == 0 and d["unique_ours"] == 0)
    d = split(relaxed([canon("mov", "0x34(%esp),%eax")]),
              relaxed([canon("xor", "0x34(%esp),%eax")]))
    check("different MNEMONIC stays unique when MASKED",
          d["unique_blob"] == 1 and d["unique_ours"] == 1)

    # A call keeps its TARGET and loses its displacement.
    c1 = canon("call", ".+70 @R_386_PC32:('symbol', 'v8_mpyint', -4)")
    c2 = canon("call", ".+393 @R_386_PC32:('symbol', 'v8_mpyint', -4)")
    c3 = canon("call", ".+70 @R_386_PC32:('symbol', 'v8_absfn', -4)")
    check("call displacement ignored, target kept", c1 == c2 and c1 != c3)

    # Known bad controls: all have different semantics despite zero surplus.
    # Exercise the real canonicalisation pipeline rather than feeding it an
    # already projected example that could hide a parser change.
    blind_spots = [
        ("dependent add/multiply order",
         [("add", "$0x1,%eax"), ("imul", "$0x2,%eax,%eax")],
         [("imul", "$0x2,%eax,%eax"), ("add", "$0x1,%eax")]),
        ("different persistent member and value",
         [("movl", "$0x1,0x4(%eax)")],
         [("movl", "$0x2,0x8(%eax)")]),
        ("opposite conditional branch",
         [("test", "%eax,%eax"), ("je", "10")],
         [("test", "%eax,%eax"), ("jne", "10")]),
    ]
    for label, original, changed in blind_spots:
        d = split(relaxed([canon(*row) for row in original]),
                  relaxed([canon(*row) for row in changed]))
        check(label + " -> zero surplus with explicit limits",
              d["unique_blob"] == 0 and d["unique_ours"] == 0
              and "remain unchecked" in shape(d))
    check("dependent-order control has different results at input 3",
          (3 + 1) * 2 == 8 and 3 * 2 + 1 == 7)

    bad = sum(not ok for _, ok in checks)
    for label, ok in checks:
        print("  %s  %s" % ("ok  " if ok else "FAIL", label))
    print("\n  %d self-test case(s), %d failure(s)" % (len(checks), bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("symbols", nargs="*", metavar="SYMBOL")
    ap.add_argument("--worst", type=int, default=0, metavar="N",
                    help="rank the N shared symbols with the most instructions "
                         "that have no counterpart on the other side")
    ap.add_argument("--self-test", action="store_true",
                    help="check surplus accounting and known projection limits")
    a = ap.parse_args()

    if a.self_test:
        return self_test()
    index = structsim.require_objects()
    if a.worst:
        return worst(index, a.worst)
    if not a.symbols:
        ap.error("give one or more symbols, or --worst N")
    for sym in a.symbols:
        measure(sym, index)
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
