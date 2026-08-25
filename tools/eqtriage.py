#!/usr/bin/env python3
"""What the NON-EXACT functions actually differ IN -- the ceiling on a grade-2 oracle.

WHY THIS EXISTS

`tools/toolchain/byteident.py` grades every symbol the blob and our period
build both define: grade 0 (EXACT, same bytes in the same places), grade 1
(REGALLOC, same instructions and operands under one consistent register
bijection), then RELOC, BYTES (same size, bytes differ) and SIZE.

Grade 2 -- "differences that change neither the result nor the execution" --
is a human judgement, and the standing proposal is to mechanise it with an IR
lifter and an SMT solver.  Before anyone builds that, somebody has to say how
much of the tree such a checker could POSSIBLY move, and in what.  That
number is what this tool produces.  It does NOT decide equivalence and it is
NOT a gate: it is `extcheck.py`'s kind of thing, a triage aid.
`docs/method/equivalence.md` is what it was written for.

TWO POPULATIONS, AND THEY ARE NOT THE SAME QUESTION

  BYTES  same byte count, bytes differ.  The two sides align
         instruction-for-instruction, so the difference can be read off
         directly and this tool classifies it exactly.
  SIZE   different byte count.  No alignment exists, so nothing here can
         classify it -- what is reported is the SHAPE of the gap (how far
         apart, same mnemonic multiset or not, x87, loops), which is what
         decides whether a lifter could attempt it at all.

A byte- or instruction-aligned comparator can only ever move the first.  A
comparator that works on lifted IR is not bound by length and could in
principle attempt both, which is the whole argument for lifting.

WHAT THE CLASSES MEAN (BYTES bucket)

Mnemonic sequences are compared first, then operands.  A difference in the
operands of an otherwise identical instruction stream is the sharp case: the
two functions execute the same instructions in the same order and differ only
in what those instructions name.

  REGX   every differing operand is a REGISTER NAME.  Same code under a
         register permutation that is not one consistent bijection -- two
         registers swapped over part of the body and not the rest.
         CLAUDE.md: register allocation is FREE.  Reclassifiable, and it
         needs no lifter -- this tool sees it, and so would a byteident whose
         bijection were per-live-range rather than per-function.
  RELSEC only a relocation TARGET differs, and one side names a SECTION.
         Finding F604: the blob's addend rides inline against a section symbol
         and ours is a named symbol with a zero addend, so the two can name
         the same thing and the printed names cannot say.  Not a difference;
         not resolvable by name either.  byteident calls this UNRESOLVED at
         the byte level and this tool uses the same word.
  RELOC  a relocation target differs and both sides name a symbol.  Real:
         the function calls or references something different.
  IMM    an IMMEDIATE differs.  A different constant is a different
         computation.  A checker that calls this equivalent is wrong.
  DISP   a memory DISPLACEMENT differs -- a different structure offset or
         stack slot.  Same argument.
  MIXED  more than one of the above.
  SCHED  the mnemonic MULTISET is the same and the ORDER differs.  617's
         class; scheduling is free, but a reorder our source's data
         dependencies would FORBID is a real defect (finding F614), so this
         needs reading and not a verdict.
  SHAPE  the mnemonic multiset differs -- an inverted branch pair, a sibling
         call against call+ret, a loop idiom, if-conversion (2411).  Finding
         F2900 classified 69 of these by hand.
  LEN    the same byte count decodes to a different NUMBER of instructions.
         Reported apart from SHAPE because it cannot be a permutation.

Two attributes are reported alongside, because they decide whether any
mechanical oracle could be trusted on a row:

  x87    the function contains an x87 instruction.  The object is
         `-mfpmath=387`, and x87 is where every lifter is weakest (CLAUDE.md;
         findings F1453 and F6203 are cases where the difference between two
         "equivalent" sequences is exactly a rounding).  A checker that
         mis-models x87 gives CONFIDENT WRONG verdicts on the functions this
         project cares most about.
  loop   the function contains a backward branch.  Symbolic execution
         terminates on straight-line code and needs an invariant or a bound
         on anything else.

THE NORMALISATION IS THIS TOOL'S OWN, AND THAT IS NOT A PREFERENCE

`byteident.insns` intends to rewrite a relocated operand to its relocation
TARGET, so that two instructions relocated against the same thing compare
equal.  IT DOES NOT: the patch loop assigns to `rows[-1]`, the last row of
the whole function, for every relocation rather than to the relocation's own
instruction -- and the last row is usually `ret`, whose operand is empty, so
`re.sub` is a no-op.  Measured over the first 400 blob symbols: 158 carry a
relocation, 210 relocations in all, and `insns()` emitted the `@` marker for
**zero** of them.  The normaliser has never fired.

That is finding F134's defect in byteident's own grade-1 test, and it is why
this tool disassembles for itself.  `--raw` selects byteident's rows
unmodified so the size of the artefact can be measured rather than asserted.
Scope: `insns()` feeds only `alpha_equal`, i.e. grade 1.  Grades 0, RELOC,
BYTES and SIZE come from `body()`/`verdict()`, a separate and correct path
that masks relocated bytes and splits section from symbol targets.  So
EXACT/BYTES/SIZE are unaffected and REGALLOC is a floor.

USAGE

    tools/eqtriage.py                    both buckets, the tables
    tools/eqtriage.py --class REGX       one class, with its diffs
    tools/eqtriage.py --diff SYMBOL      the aligned diff for one symbol
    tools/eqtriage.py --raw              byteident's normalisation, for A/B
    tools/eqtriage.py --selftest         the classifier, shown to fire

    BLOB=/abs/path/dsplibs.o TC_OUT=build/tc_out tools/eqtriage.py

DENOMINATOR.  Printed on every line that carries a verdict, per findings
F2400, F2401 and F3100.
"""

import argparse
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "toolchain"))

import byteident as bi   # noqa: E402  -- after the path insert, deliberately

SECTIONS = (".text", ".rodata", ".data", ".bss", ".gnu.linkonce")


# x87 mnemonics are exactly the ones beginning with `f` on this instruction
# set.  No integer i386 mnemonic does, and the object has no SSE and no
# 3DNow -- finding F612 bounds the instruction set at `-march=i386`, no `cmov`
# in 1.2 MB.  `fwait` and `fnstsw` count: they are x87 control, and a lifter
# that drops them drops the status word.
def is_x87(mnem):
    return mnem.startswith("f")


JCC = re.compile(r"^j|^loop")
NUM = re.compile(r"0x[0-9a-f]+|(?<=\$)-?\d+")
IMMTOK = re.compile(r"\$(0x[0-9a-f]+|-?\d+|@\S+)")
BAREHEX = re.compile(r"[0-9a-f]+")
RELTOK = re.compile(r"@[\w.$]+")


def insns(path, sym):
    """[(mnemonic, operands)] with the relocation normalisation ACTUALLY applied.

    One pass, so a relocation patches the instruction it belongs to -- objdump
    prints it on the line immediately after.

    TWO SUBSTITUTION RULES, AND THE SECOND MUST STAY LITERAL-ONLY.

      bare-hex operand  the whole operand becomes `@<target>`.  This is the
                        case byteident's number-only rule could not reach: a
                        relocated `call` prints its target as the NEXT
                        ADDRESS (addend -4), so `call foo` and `call bar`
                        both normalise to the same self-relative offset and
                        compare EQUAL.
      anything else     only the NUMERIC LITERALS become `@<target>`, exactly
                        as byteident intended.

    Replacing the whole operand in the second case is an over-normalisation
    and it was in this tool for one revision: it collapses `mov %eax,glob`
    and `mov glob,%eax` -- a store and a load -- to the same row.  Registers,
    direction and addressing mode all have to survive.  `--selftest` has a
    control for it.
    """
    out = subprocess.run(
        ["objdump", "-dr", "--disassemble=" + sym, "--no-show-raw-insn", path],
        capture_output=True, text=True).stdout
    rows, offs, bare, base = [], [], [], None
    for line in out.splitlines():
        m = re.match(r"^\s*([0-9a-f]+):\t", line)
        if m:
            at = int(m.group(1), 16)
            if base is None:
                base = at
            body = line.split("\t", 1)[1].strip()
            body = body.split("#", 1)[0].strip()
            body = re.sub(r"<[^>]*>", "", body).strip()
            parts = body.split(None, 1)
            ops = parts[1].strip() if len(parts) > 1 else ""
            bare.append(bool(BAREHEX.fullmatch(ops)))
            # A branch target is an ABSOLUTE address and the two objects put
            # the function at different offsets, so `jmp 7e321` and `jmp f1`
            # are the same jump printed twice.  byteident's correction, kept.
            if bare[-1]:
                ops = ".%+d" % (int(ops, 16) - base)
            rows.append([parts[0], ops])
            offs.append(at - base)
            continue
        m = bi.RELOC.match(line)
        if m and rows:
            tgt = "@" + m.group(3)
            rows[-1][1] = tgt if bare[-1] else NUM.sub(tgt, rows[-1][1])
    return [tuple(r) for r in rows], offs


def has_loop(rows, offs):
    """A branch whose target is at or before the branching instruction.

    Branch operands are normalised to `.%+d` from the function entry, so the
    target compares directly against the instruction's own offset.  An
    indirect or relocated jump has no such operand and is not counted; it is
    reported by `--diff` and read by hand.
    """
    for (mnem, ops), at in zip(rows, offs):
        if not JCC.match(mnem):
            continue
        m = re.fullmatch(r"\.([-+]\d+)", ops)
        if m and int(m.group(1)) <= at:
            return True
    return False


def diff_kind(ox, oy):
    """What differs between two operand strings of the SAME instruction.

    A relocation target now sits INSIDE the operand (`$@v8_costab,%eax`), so
    the two axes are separated: the `@` tokens are compared as one question
    and everything around them -- registers, direction, addressing mode -- as
    another.  A difference in both is MIXED, which is what keeps a relocated
    load from comparing equal to a relocated store against the same symbol.
    """
    if ox == oy:
        return None
    rx, ry = RELTOK.findall(ox), RELTOK.findall(oy)
    bx, by = RELTOK.sub("@", ox), RELTOK.sub("@", oy)
    rel_diff, rest_diff = rx != ry, bx != by
    if rel_diff:
        if rest_diff:
            return "MIXED"
        if len(rx) == len(ry) and any(
                x != y and (x[1:].startswith(SECTIONS)
                            or y[1:].startswith(SECTIONS))
                for x, y in zip(rx, ry)):
            return "RELSEC"      # finding F604: cannot be compared by name
        return "RELOC"
    ox, oy = bx, by
    imm_diff = IMMTOK.findall(ox) != IMMTOK.findall(oy)
    num_diff = NUM.findall(ox) != NUM.findall(oy)
    mx, my = NUM.sub("#", ox), NUM.sub("#", oy)
    reg_diff = mx != my
    if num_diff and reg_diff:
        return "MIXED"
    if num_diff:
        return "IMM" if imm_diff else "DISP"
    if reg_diff:
        return "REG" if bi.REGTOK.sub("%r", mx) == bi.REGTOK.sub("%r", my) \
            else "MIXED"
    return "MIXED"


def classify(a, b):
    """(class, [(index, mnemonic, blob_ops, our_ops, kind)]) for two row lists."""
    if len(a) != len(b):
        return "LEN", []
    if [m for m, _ in a] != [m for m, _ in b]:
        if sorted(m for m, _ in a) == sorted(m for m, _ in b):
            return "SCHED", []
        return "SHAPE", []
    diffs, kinds = [], set()
    for i, ((mx, ox), (_, oy)) in enumerate(zip(a, b)):
        k = diff_kind(ox, oy)
        if k:
            diffs.append((i, mx, ox, oy, k))
            kinds.add(k)
    if not kinds:
        # Identical mnemonics AND identical normalised operands, yet the
        # BYTES differ.  An ENCODING difference: two encodings of one
        # instruction, or alignment padding.  Its own class, because calling
        # it a register permutation would be exactly the normalisation
        # artefact this document is about.
        return "ENCODING", diffs
    if kinds == {"REG"}:
        return "REGX", diffs
    if kinds == {"RELSEC"}:
        return "RELSEC", diffs
    if kinds <= {"REG", "RELSEC"}:
        return "REGX", diffs         # free + unresolvable, nothing real in it
    if kinds == {"IMM"}:
        return "IMM", diffs
    if kinds == {"DISP"}:
        return "DISP", diffs
    if kinds == {"RELOC"}:
        return "RELOC", diffs
    return "MIXED", diffs


ORDER = ["REGX", "RELSEC", "ENCODING", "IMM", "DISP", "RELOC", "MIXED",
         "SCHED", "SHAPE", "LEN"]
MOVABLE = {"REGX", "RELSEC", "ENCODING"}
NEEDS_READING = {"SCHED", "SHAPE"}
REAL = {"IMM", "DISP", "RELOC", "MIXED", "LEN"}


def selftest():
    """Show the classifier firing, in both directions.  Finding F134.

    The classifier is the only new logic -- the disassembly is objdump's.  So
    the controls are synthetic row lists, which is what lets them be exact: a
    real-function control could only ever show that SOMETHING was classified.
    A control for each class, plus the two attribute detectors.
    """
    base = [("mov", "%eax,%edx"), ("add", "$0x10,%edx"),
            ("mov", "0x24(%esi),%ecx"), ("jne", ".+40"), ("fldl", "(%ecx)")]
    cases = [
        ("identical to itself",     base, base, "ENCODING"),
        ("one register swapped",
         base, [("mov", "%eax,%ebx")] + base[1:], "REGX"),
        ("one immediate off by one",
         base, base[:1] + [("add", "$0x11,%edx")] + base[2:], "IMM"),
        ("one displacement moved",
         base, base[:2] + [("mov", "0x28(%esi),%ecx")] + base[3:], "DISP"),
        ("two instructions swapped",
         base, [base[1], base[0]] + base[2:], "SCHED"),
        ("one mnemonic replaced",
         base, base[:3] + [("je", ".+40")] + base[4:], "SHAPE"),
        ("one instruction dropped", base, base[:4], "LEN"),
        ("a register AND an immediate",
         base, [("mov", "%eax,%ebx"), ("add", "$0x11,%edx")] + base[2:],
         "MIXED"),
    ]
    #
    # The relocated controls.  The LAST of them is the one that matters: a
    # relocated load and a relocated store against the SAME symbol must be a
    # difference.  An earlier revision of insns() replaced the whole operand
    # with `@target` and collapsed the two, and nothing here could see it --
    # the other controls feed operands that are already bare `@target`, so
    # they never exercise what the substitution threw away.
    #
    rel = [("call", "@modem_dp_deregister"), ("mov", "$@.rodata.str1.1,%eax")]
    cases += [
        ("a section reloc vs a symbol",
         rel, [rel[0], ("mov", "$@v8_costab,%eax")], "RELSEC"),
        ("two symbol relocs differing",
         rel, [("call", "@other_fn"), rel[1]], "RELOC"),
        ("relocated LOAD vs STORE, same symbol",
         rel, [rel[0], ("mov", "%eax,$@.rodata.str1.1")], "MIXED"),
    ]
    bad = 0
    print("classifier self-test -- %d control(s)\n" % (len(cases)))
    for label, left, right, want in cases:
        got, _ = classify(left, right)
        ok = got == want
        bad += not ok
        print("  %-38s want %-9s got %-9s %s"
              % (label, want, got, "ok" if ok else "MISMATCH"))
    print("\n  x87 detector : fldl=%s fmulp=%s fnstsw=%s mov=%s add=%s"
          % tuple(is_x87(m) for m in ("fldl", "fmulp", "fnstsw", "mov", "add")))
    fwd = ([("jne", ".+40"), ("nop", "")], [0, 6])
    back = ([("nop", ""), ("jne", ".-4")], [0, 1])
    print("  loop detector: forward=%s backward=%s"
          % (has_loop(*fwd), has_loop(*back)))
    #
    # AND THE NORMALISER ITSELF, on a real function, against the tool it
    # corrects.  `dp_v23_exit` carries two relocations: R_386_32 .data on
    # `mov $0x60,%eax` and R_386_PC32 modem_dp_deregister on the call.
    #
    try:
        rows, _ = insns(bi.BLOB, "dp_v23_exit")
        raw = bi.insns(bi.BLOB, "dp_v23_exit")
        mine = sum(1 for _, o in rows if "@" in o)
        theirs = sum(1 for _, o in raw if "@" in o)
        print("  reloc normaliser on dp_v23_exit: this tool %d, "
              "byteident.insns %d  (2 relocations exist)" % (mine, theirs))
        bad += (mine != 2)
    except Exception as e:                                  # no blob present
        print("  reloc normaliser: SKIPPED (%s)" % e)
    print("\n%d control(s) MISCLASSIFIED" % bad)
    return 1 if bad else 0


def load():
    blob = bi.sizes(bi.BLOB)
    if not blob:
        sys.exit("eqtriage.py: NO SYMBOLS read from the blob at %s -- every\n"
                 "  count below would be computed against nothing.  From a\n"
                 "  worktree, BLOB must be explicit.  Findings F2400, F2401."
                 % bi.BLOB)
    ours = {}
    for o in sorted(glob.glob(os.path.join(bi.OURS, "*.o"))):
        for k in bi.sizes(o):
            ours.setdefault(k, o)
    if not ours:
        sys.exit("eqtriage.py: no objects in %s -- run "
                 "tools/toolchain/build.sh first." % bi.OURS)
    common = sorted(k for k in ours if k in blob)
    if not common:
        sys.exit("eqtriage.py: blob and %s share NO symbols; the denominator "
                 "is zero, which is not a score." % bi.OURS)
    return blob, ours, common


def collect(raw=False):
    blob, ours, common = load()
    bytes_rows, size_rows = [], []
    for k in common:
        ab, ar = bi.body(bi.BLOB, k)
        bb, br = bi.body(ours[k], k)
        v, _ = bi.verdict(ab, ar, bb, br)
        if v not in ("BYTES", "SIZE"):
            continue
        #
        # THE ATTRIBUTES ARE ALWAYS COMPUTED FROM THE CORRECTED ROWS, even
        # under --raw.  x87 and loops are properties of the FUNCTION, not of
        # a normalisation, and taking them from the raw rows made the loop
        # column read a silent zero for every row -- a dead column in a table
        # printed to be believed, which is findings F2400 and F3100 exactly.
        # --raw changes what is CLASSIFIED and nothing else, which is what
        # makes the A/B isolate the normaliser.
        #
        ia, oa = insns(bi.BLOB, k)
        ib, _ = insns(ours[k], k)
        x87 = any(is_x87(m) for m, _ in ia) or any(is_x87(m) for m, _ in ib)
        lp = has_loop(ia, oa)
        if raw:
            ia, ib = bi.insns(bi.BLOB, k), bi.insns(ours[k], k)
        if v == "BYTES":
            if bi.alpha_equal(ia, ib):
                bytes_rows.append((k, blob[k], "ALPHA", [], x87, lp))
                continue
            cls, diffs = classify(ia, ib)
            bytes_rows.append((k, blob[k], cls, diffs, x87, lp))
        else:
            #
            # (sym, blob bytes, our bytes, blob insns, our insns,
            #  x87, loop, same mnemonic multiset)
            #
            same = sorted(m for m, _ in ia) == sorted(m for m, _ in ib)
            size_rows.append((k, len(ab), len(bb), len(ia), len(ib),
                              x87, lp, same))
    return bytes_rows, size_rows, len(common)


def table(rows, n, total):
    by = {}
    for r in rows:
        by.setdefault(r[2], []).append(r)
    print("  class      n   of %-4d  x87  loop  a grade-2 oracle could..." % n)
    print("  ------------------------------------------------------------------")
    for c in ["ALPHA"] + ORDER:
        rs = by.get(c, [])
        if not rs:
            continue
        note = ("RECLASSIFY -- grade 1 already, byteident's dead"
                if c == "ALPHA" else
                "RECLASSIFY -- free or unresolvable" if c in MOVABLE else
                "not decide -- read by hand" if c in NEEDS_READING else
                "only be WRONG -- real difference")
        print("  %-9s %3d  %5.1f%%   %3d  %4d  %s"
              % (c, len(rs), 100.0 * len(rs) / n,
                 sum(1 for r in rs if r[4]), sum(1 for r in rs if r[5]), note))
    print("  ------------------------------------------------------------------")
    mv = sum(len(by.get(c, [])) for c in MOVABLE | {"ALPHA"})
    rd = sum(len(by.get(c, [])) for c in NEEDS_READING)
    rl = sum(len(by.get(c, [])) for c in REAL)
    print("  movable   %3d  %5.1f%% of the %d, %.2f%% of the %d compared"
          % (mv, 100.0 * mv / n, n, 100.0 * mv / total, total))
    print("  to read   %3d  %5.1f%% of the %d" % (rd, 100.0 * rd / n, n))
    print("  real      %3d  %5.1f%% of the %d" % (rl, 100.0 * rl / n, n))
    return by


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--raw", action="store_true",
                    help="byteident's normalisation, unmodified, for A/B")
    ap.add_argument("--class", dest="cls", help="print only this class")
    ap.add_argument("--diff", help="the aligned diff for one symbol")
    ap.add_argument("--limit", type=int, default=40)
    a = ap.parse_args()

    if a.selftest:
        return selftest()

    if a.diff:
        _, ours, _ = load()
        if a.diff not in ours:
            sys.exit("eqtriage.py: %s is not defined in %s" % (a.diff, bi.OURS))
        ia, _ = insns(bi.BLOB, a.diff)
        ib, _ = insns(ours[a.diff], a.diff)
        cls, _ = classify(ia, ib)
        print("%s -- %s, %d blob insn(s), %d ours\n"
              % (a.diff, cls, len(ia), len(ib)))
        for i in range(max(len(ia), len(ib))):
            x = "%-8s %s" % ia[i] if i < len(ia) else ""
            y = "%-8s %s" % ib[i] if i < len(ib) else ""
            print("  %s %-42s | %s" % ("!" if x != y else " ", x, y))
        return 0

    brows, srows, total = collect(a.raw)
    nb, ns = len(brows), len(srows)
    if not nb and not ns:
        sys.exit("eqtriage.py: byteident's BYTES and SIZE buckets are both "
                 "EMPTY over %d symbol(s).  Check TC_OUT." % total)

    print("  blob    : %s" % bi.BLOB)
    print("  ours    : %s" % bi.OURS)
    print("  normalis: %s" % ("byteident.insns UNMODIFIED (--raw)" if a.raw
                              else "this tool's, relocations applied"))
    print("\nNon-exact over %d symbol(s) both define: "
          "BYTES %d (%.1f%%) + SIZE %d (%.1f%%) = %d (%.1f%%)"
          % (total, nb, 100.0 * nb / total, ns, 100.0 * ns / total,
             nb + ns, 100.0 * (nb + ns) / total))

    print("\n=== BYTES -- same size, bytes differ.  %d of %d (%.1f%%) ==="
          % (nb, total, 100.0 * nb / total))
    print("A byte- or instruction-aligned comparator can move only this.\n")
    by = table(brows, nb, total)

    nox = [r for r in brows if not r[4] and not r[5]]
    print("\n  no x87 and no loop: %d of %d (%.1f%%), %.2f%% of %d compared"
          % (len(nox), nb, 100.0 * len(nox) / nb,
             100.0 * len(nox) / total, total))
    print("  x87 anywhere      : %d of %d (%.1f%%)"
          % (sum(1 for r in brows if r[4]), nb,
             100.0 * sum(1 for r in brows if r[4]) / nb))

    if srows:
        print("\n=== SIZE -- different byte count.  %d of %d (%.1f%%) ==="
              % (ns, total, 100.0 * ns / total))
        print("No alignment exists, so nothing here is classified.  A lifted")
        print("comparison is not bound by length and could ATTEMPT these;")
        print("what follows is how far apart they are.\n")
        band = [("within 4 bytes", 0, 4), ("5-16", 5, 16), ("17-64", 17, 64),
                ("65-256", 65, 256), ("over 256", 257, 10 ** 9)]
        print("  |blob - ours|      n   of %-4d  x87  loop  same mnemonic"
              " multiset" % ns)
        print("  ------------------------------------------------------------"
              "------")
        for lab, lo, hi in band:
            rs = [r for r in srows if lo <= abs(r[1] - r[2]) <= hi]
            if not rs:
                continue
            print("  %-16s %4d  %5.1f%%   %3d  %4d  %d"
                  % (lab, len(rs), 100.0 * len(rs) / ns,
                     sum(1 for r in rs if r[5]), sum(1 for r in rs if r[6]),
                     sum(1 for r in rs if r[7])))
        print("  ------------------------------------------------------------"
              "------")
        print("  same mnemonic multiset, different bytes: %d of %d (%.1f%%)"
              % (sum(1 for r in srows if r[7]), ns,
                 100.0 * sum(1 for r in srows if r[7]) / ns))
        print("  ours LARGER %d, ours SMALLER %d, blob total %d B, ours %d B"
              % (sum(1 for r in srows if r[2] > r[1]),
                 sum(1 for r in srows if r[2] < r[1]),
                 sum(r[1] for r in srows), sum(r[2] for r in srows)))
        print("  x87 anywhere in SIZE : %d of %d (%.1f%%)"
              % (sum(1 for r in srows if r[5]), ns,
                 100.0 * sum(1 for r in srows if r[5]) / ns))
        print("  loop  anywhere       : %d of %d (%.1f%%)"
              % (sum(1 for r in srows if r[6]), ns,
                 100.0 * sum(1 for r in srows if r[6]) / ns))
        clean = [r for r in srows if not r[5] and not r[6]]
        print("  neither              : %d of %d (%.1f%%), %.2f%% of %d"
              % (len(clean), ns, 100.0 * len(clean) / ns,
                 100.0 * len(clean) / total, total))

    want = [a.cls] if a.cls else ORDER + ["ALPHA"]
    for c in want:
        rs = by.get(c, [])
        if not rs:
            continue
        print("\n%s -- %d of %d:" % (c, len(rs), nb))
        lim = a.limit if a.limit else len(rs)
        for k, sz, _, diffs, x87, lp in sorted(rs, key=lambda r: r[1])[:lim]:
            tags = ("x" if x87 else "-") + ("L" if lp else "-")
            print("  %5d  %s  %s%s" % (sz, tags, k,
                                       "  (%d insn differ)" % len(diffs)
                                       if diffs else ""))
            if a.cls:
                for i, m, ox, oy, kind in diffs[:8]:
                    print("           %-6s #%-4d %-8s %-28s | %s"
                          % (kind, i, m, ox, oy))
        if len(rs) > lim:
            print("  ... %d more" % (len(rs) - lim))
    return 0


if __name__ == "__main__":
    sys.exit(main())
