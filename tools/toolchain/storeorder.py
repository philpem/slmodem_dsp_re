#!/usr/bin/env python3
"""Where does our STATEMENT ORDER differ from the original's?

A DIFFERENCE HERE IS A HINT, NOT A CONCLUSION, and the distinction cost a
false premise once already (finding F617).  GCC does NOT simply preserve source
order: `toneiir_reset`'s source is already in the object's order and the
compiler emits ours in a different one, hoisting a short store and sinking an
int store.  So you cannot read the author's statement order off the object.

What a difference here means is only that SOMETHING upstream differs.  The
acceptance test for any reorder is FULL-TEXT IDENTITY -- operands included --
not agreement of this list.  Where permuting our statements makes the whole
function match the object exactly, that is the author's order recovered.  Where
it only shuffles this list, it is noise and must be left alone.

That is recoverable source, and worth recovering: ours are frequently sorted by
struct offset, which is a tidiness we imposed, and it erases whatever grouping
the author had.  A comment pass that writes "initialise the filter state" over
a list we sorted is describing our sorting.  Findings F615 and F617.

Only stores through a register holding a pointer argument are considered, and
only functions where the two SETS agree -- a different set is a different
question (missing or extra work), not a different order.

    tools/toolchain/storeorder.py [--all]

Without --all, only functions whose order differs are listed.

WHAT IT SURFACES, AND HOW MUCH OF IT IS WORTH TOUCHING (finding F2402).  On the
current toolchain it reports 57 differing functions out of 194 eligible and 938
shared symbols.  MOST OF THAT IS NOT ACTIONABLE, and the tool now says which
part is: only 14 of the 57 have bodies whose MNEMONICS already match, which is
the precondition for 617's full-text test ever passing.  The other 43 differ in
more than order, and permuting their statements would be fitting the compiler.
617's own rate on the 19 it examined by hand was two.

So read a run as "14 worth a look, 43 to leave alone", never as 57 defects.
"""

import glob
import os
import re
import subprocess
import sys

BLOB = os.environ.get("BLOB", "ref/slmodemd/dsplibs.o")
OURS = os.environ.get("TC_OUT", "build/tc_out")


def sizes(path):
    d = {}
    for line in subprocess.run(["nm", "--size-sort", "-S", path],
                               capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "TtWw":
            d[p[3]] = (int(p[1], 16), path)
    return d


def store_seq(path, sym):
    """Ordered destination offsets of stores through a base register."""
    out = subprocess.run(
        ["objdump", "-d", "--disassemble=" + sym, "--no-show-raw-insn", path],
        capture_output=True, text=True).stdout
    seq = []
    for line in out.splitlines():
        m = re.search(r":\s+mov[lwb]?\s+\S+,(0x[0-9a-f]+)\(%e[a-d]x\)\s*$", line)
        if m:
            seq.append(m.group(1))
    return seq


PAD = re.compile(r"nop|lea 0x0\(.*\),%e[a-z][a-z]$|mov %e(si|di),%e(si|di)$")


def body(path, sym):
    """Instructions with alignment padding removed.

    Multi-byte NOPs are spelled as `lea 0x0(%esi,%eiz,1),%esi` and
    `mov %esi,%esi`, which read as ordinary instructions.  Under -O3 the
    scheduler emits far more of them, and leaving them in makes two identical
    bodies compare unequal.  Finding F2401.
    """
    out = subprocess.run(
        ["objdump", "-d", "--disassemble=" + sym, "--no-show-raw-insn", path],
        capture_output=True, text=True).stdout
    r = []
    for line in out.splitlines():
        m = re.match(r"^\s*[0-9a-f]+:\s+(.*?)\s*$", line)
        if m:
            t = re.sub(r"\s*<[^>]*>", "",
                       re.sub(r"\s+", " ", m.group(1))).strip()
            if not PAD.match(t):
                r.append(t)
    return r


def eligibility(blob_path, our_path, sym):
    """Can this candidate possibly pass 617's full-text test?

    617's acceptance test is FULL-TEXT identity, operands included, and it is
    not a formality: of nineteen functions examined there, seventeen failed.
    Where the two bodies already agree MNEMONIC for mnemonic, only operands and
    order stand in the way and permuting statements can close it.  Where the
    mnemonics differ too, something bigger differs upstream and the store order
    is a symptom -- permuting there is fitting the compiler, which 617 forbids.
    Finding F2402.
    """
    a, b = body(blob_path, sym), body(our_path, sym)
    if a == b:
        return "ALREADY FULL-TEXT IDENTICAL"
    if [x.split()[0] for x in a] == [x.split()[0] for x in b]:
        return "ELIGIBLE -- mnemonics already match"
    return ""


def main():
    show_all = "--all" in sys.argv
    blob = sizes(BLOB)

    # AN EMPTY SET IS NOT A CLEAN TREE.  This defaulted to `/tmp/tc_out` after
    # the period build moved to `build/tc_out`, so it globbed nothing, compared
    # and printed "0 functions differ" -- which reads as good news.  Finding
    # 2400.
    objs = sorted(glob.glob(os.path.join(OURS, "*.o")))
    if not objs:
        sys.exit("storeorder: no objects in TC_OUT=%s -- run `make tc` "
                 "first.  Refusing to report a clean tree that was "
                 "never examined." % OURS)
    ours = {}
    for o in objs:
        for k, v in sizes(o).items():
            ours.setdefault(k, v)

    diff = same = eligible_ft = 0
    compared = eligible = 0
    for k in sorted(ours):
        if k not in blob:
            continue
        compared += 1
        a, b = store_seq(BLOB, k), store_seq(ours[k][1], k)
        if len(a) < 2 or sorted(a) != sorted(b):
            continue
        eligible += 1
        if a == b:
            same += 1
            if show_all:
                print("  same   %s" % k)
            continue
        diff += 1
        elig = eligibility(BLOB, ours[k][1], k)
        if elig.startswith("ELIGIBLE") or elig.startswith("ALREADY"):
            eligible_ft += 1
        ascending = b == sorted(b, key=lambda x: int(x, 16))
        note = "; ".join(x for x in
                         ("ours is SORTED BY OFFSET" if ascending else "", elig)
                         if x)
        print("  %-42s %s" % (k, note))
        print("      object: %s" % " ".join(a))
        print("      ours:   %s" % " ".join(b))
    print("\n  %d functions differ in store order, %d already agree" % (diff, same))
    #
    # MOST WILL NOT SURVIVE, and the tool should say so rather than let the
    # count read as a defect count.  617: nineteen examined, two passed.
    #
    print("  of the %d, %d are ELIGIBLE for 617's full-text test (mnemonics\n"
          "     already match); the other %d differ in mnemonics too and 617's\n"
          "     rule is to leave those alone." % (diff, eligible_ft, diff - eligible_ft))
    print("  %d symbols compared, %d eligible (>=2 visible stores, same set)"
          "   TC_OUT=%s" % (compared, eligible, OURS))
    #
    # SAY WHAT THE REGEX CANNOT SEE.  `store_seq` matches only a store through
    # %eax/%ebx/%ecx/%edx with a NON-ZERO hex displacement, so a store at
    # offset 0, and any store through %esi/%edi/%ebp, is invisible.  Under
    # -O3 -frename-registers the allocator uses the extra registers freely, so
    # a function can be "eligible" on a partial view of its stores.  Widening
    # this changes what the tool surfaces and needs the fire/quiet validation
    # run again; it has not been done.  Finding F2402.
    #
    print("  NOTE: sees stores through %eax/%ebx/%ecx/%edx at a non-zero\n"
          "        displacement only; other bases and offset 0 are invisible.")


if __name__ == "__main__":
    sys.exit(main() or 0)
