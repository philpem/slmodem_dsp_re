#!/usr/bin/env python3
"""Triage the SAME SIZE, DIFFERENT INSTRUCTIONS slice of `compare.py`.

`compare.py` splits the symbols the blob and this tree both have into three
buckets: identical mnemonic sequences, same byte count with a different
sequence, and different size.  It PRINTS the first bucket and counts the
other two.  This tool prints the second one, which is the sharpest slice in
the object: same byte count means nothing is missing and nothing is extra, so
whatever differs is a spelling, an order or a shape inside a function that is
otherwise the right size.

WHAT THIS TOOL IS NOT.  It is a triage aid, never a gate and never a defect
list -- the same standing `storeorder.py` and `extcheck.py` have.  Its tags
are lexical: they say what SHAPE the difference has, not whether the source is
wrong.  Every row still has to be read against `tools/dis.py` before anything
is edited, and CLAUDE.md's rule decides:

    Act on what the compiler was FORCED to encode.
    Ignore what it was free to choose.

THE FREE COLUMN IS NARROWER HERE THAN IT IS IN GENERAL, and that is the point
of this slice.  `compare.py` compares MNEMONICS with operands dropped, so a
difference that is only register allocation, only a stack slot or only a
relocated address ALREADY SCORES AS IDENTICAL and is inside the identical
bucket.  Nothing reaching this list can be pure regalloc.  What is still free
in it:

  - scheduling -- the same multiset of mnemonics in a different order (tag
    SCHED)
  - the extension on a load whose upper half is discarded (finding 614)
  - if-conversion of an INTEGER two-constant select: an integer compare has no
    unordered case, so the branch form and the branchless form compute the
    same function over every input and no differential test can ever separate
    them.  Unpinnable by construction -- finding 2411

and what is forced:

  - the signedness of a load whose 32-bit result is USED (finding 613)
  - a branchless select over a FLOAT compare, where the unordered case routes
    the two spellings to different arms (findings 2300, 2410)

A DETECTOR MUST REPORT ITS DENOMINATOR.  Both of the other aids here spent a
period defaulting to an absent `TC_OUT`, comparing zero symbols and printing a
clean tree at exit 0 (finding 2400).  This one refuses an empty `TC_OUT` and
prints how many symbols it compared on every run.

Usage:

    BLOB=.../dsplibs.o TC_OUT=$PWD/build/tc_out tools/samesize.py
    ... --sym NAME      the aligned mnemonic diff for one symbol
    ... --names         just the symbol names, one per line
    ... --identical     the IDENTICAL set's names, for diffing a before
                        against an after -- a count can gain four and lose
                        four and not move (finding 2155)
"""

import argparse
import glob
import os
import re
import subprocess
import sys
from collections import Counter

BLOB = os.environ.get("BLOB", "ref/slmodemd/dsplibs.o")
OURS = os.environ.get("TC_OUT", "")

#
# WHO OWNS WHAT, while five sessions are live in parallel worktrees.  A row
# landing in someone else's file is REPORTED, not edited.  Paths are matched
# as prefixes against the manifest's source name; bare names are matched
# against the symbol.
#
OWNED = {
    "src/pump/v34/v34hshak.c": "agent-debugsites",
    "src/callprog/": "agent-debugsites",
    "src/dialer/": "agent-debugsites",
    "src/v8/": "agent-debugsites",
    "src/core/fixedrc.c": "agent-rcfixed",
}
OWNED_SYMS = {
    "V34scrambler": "agent-mirror",
    "V34descrambler": "agent-mirror",
    "dpskinit": "agent-mirror",
    "receiver": "agent-mirror",
    "findPadGain": "agent-mirror",
    "bitsToInfo": "agent-mirror",
}

# The x87 compare family.  `-mno-ieee-fp` (finding 1990) makes these ORDERED,
# so a difference in one is about the source's comparison and not the flag.
FCOM = ("fcom", "fcoms", "fcomp", "fcomps", "fcompp", "fcoml", "fcompl",
        "fucom", "fucomp", "fucompp")
EXT = ("movzwl", "movswl", "movzbl", "movsbl", "movzbw", "movsbw", "cwtl",
       "cbtw", "cltd", "cwtd")


def sizes(path):
    out = subprocess.run(["nm", "--size-sort", "-S", path],
                         capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "TtWw":
            d[p[3]] = int(p[1], 16)
    return d


def disasm(path, sym):
    """(mnemonics, full text lines) for one symbol."""
    out = subprocess.run(
        ["objdump", "-d", "--disassemble=" + sym, "--no-show-raw-insn", path],
        capture_output=True, text=True).stdout
    mn, txt = [], []
    for l in out.splitlines():
        m = re.match(r"^\s*[0-9a-f]+:\s+(\S+)(.*)$", l)
        if m:
            mn.append(m.group(1))
            txt.append((m.group(1) + " " + m.group(2).strip()).strip())
    return mn, txt


def owner(src, sym):
    for pfx, who in OWNED.items():
        if src.startswith(pfx):
            return who
    for name, who in OWNED_SYMS.items():
        if name in sym:
            return who
    return ""


def classify(bm, om):
    """A lexical SHAPE tag and the evidence for it.  Never a verdict."""
    bc, oc = Counter(bm), Counter(om)
    tags = []
    if bc == oc:
        # Same instructions, different order.  Nothing was added or removed,
        # so this is scheduling or statement order -- 617's "in between".
        return "SCHED", "same multiset of %d mnemonics, different order" % len(bm)

    only_b = bc - oc
    only_o = oc - bc

    def has(c, names):
        return sum(c[n] for n in names if n in c)

    # A branchless select against a branch: the 2410 shape.  Forced when the
    # compare is a FLOAT one, free when it is integer (2411).
    if (has(only_b, ("sbb",)) or has(only_o, ("sbb",))):
        side = "blob" if has(only_b, ("sbb",)) else "ours"
        fl = "float" if (has(bc, FCOM) or has(oc, FCOM)) else "integer"
        tags.append(("SELECT", "sbb only in %s; compare is %s (2410/2411)"
                     % (side, fl)))

    # Extension width.  Forced only where the 32-bit result is USED (613);
    # free where the upper half is discarded (614).
    if has(only_b, EXT) or has(only_o, EXT):
        b = " ".join("%s x%d" % (k, v) for k, v in sorted(only_b.items())
                     if k in EXT)
        o = " ".join("%s x%d" % (k, v) for k, v in sorted(only_o.items())
                     if k in EXT)
        tags.append(("EXT", "blob-only[%s] ours-only[%s] (613/614)"
                     % (b or "-", o or "-")))

    # A float compare present on one side only, or a different one.
    if has(only_b, FCOM) or has(only_o, FCOM):
        b = " ".join("%s x%d" % (k, v) for k, v in sorted(only_b.items())
                     if k in FCOM)
        o = " ".join("%s x%d" % (k, v) for k, v in sorted(only_o.items())
                     if k in FCOM)
        tags.append(("FCMP", "blob-only[%s] ours-only[%s] (1990/2300)"
                     % (b or "-", o or "-")))

    # Conditional-jump sense.  Same COUNT of jumps but different conditions is
    # an operand-order or negation question; 1991 is the refutation of reading
    # it as one.
    jb = Counter({k: v for k, v in bc.items()
                  if k.startswith("j") and k != "jmp"})
    jo = Counter({k: v for k, v in oc.items()
                  if k.startswith("j") and k != "jmp"})
    if jb != jo and sum(jb.values()) == sum(jo.values()):
        tags.append(("JCC", "same %d conditional jumps, senses differ: blob[%s]"
                     " ours[%s] (1991)"
                     % (sum(jb.values()),
                        " ".join("%s x%d" % (k, v) for k, v in sorted((jb - jo).items())),
                        " ".join("%s x%d" % (k, v) for k, v in sorted((jo - jb).items())))))

    if not tags:
        b = " ".join("%s x%d" % (k, v) for k, v in sorted(only_b.items()))
        o = " ".join("%s x%d" % (k, v) for k, v in sorted(only_o.items()))
        return "OTHER", "blob-only[%s] ours-only[%s]" % (b or "-", o or "-")
    return ("+".join(t for t, _ in tags),
            "; ".join(r for _, r in tags))


def collect():
    if not OURS or not os.path.isdir(OURS):
        sys.exit("samesize: TC_OUT is empty or absent (%r).  Run\n"
                 "  TC_OUT=$PWD/build/tc_out tools/toolchain/build.sh\n"
                 "first -- finding 2400 is what happens when this is skipped."
                 % OURS)
    blob = sizes(BLOB)
    ours, objs = {}, sorted(glob.glob(os.path.join(OURS, "*.o")))
    if not objs:
        sys.exit("samesize: no objects in TC_OUT=%s" % OURS)
    for o in objs:
        for k, v in sizes(o).items():
            ours.setdefault(k, (v, o))
    manifest = {}
    try:
        for line in open(os.path.join(OURS, "tc_manifest.txt")):
            a, b = line.split()
            manifest[a] = b
    except OSError:
        pass
    common = sorted(k for k in ours if k in blob)
    same, ident = [], []
    for k in common:
        o, path = ours[k]
        if blob[k] != o:
            continue
        bm, _ = disasm(BLOB, k)
        om, _ = disasm(path, k)
        src = manifest.get(os.path.basename(path), os.path.basename(path))
        if bm == om:
            ident.append(k)
        else:
            same.append((o, k, src, path, bm, om))
    return common, ident, same


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sym", help="aligned mnemonic diff for one symbol")
    ap.add_argument("--all", action="store_true",
                    help="every symbol's diff, in ONE pass.  Looping --sym "
                         "over --names re-disassembles the whole set per "
                         "symbol and is quadratic; this is not")
    ap.add_argument("--names", action="store_true")
    ap.add_argument("--identical", action="store_true",
                    help="print the IDENTICAL set's names, for set diffing")
    args = ap.parse_args()

    common, ident, same = collect()

    if args.identical:
        for k in sorted(ident):
            print(k)
        return 0
    if args.names:
        for _, k, _, _, _, _ in sorted(same):
            print(k)
        return 0

    if args.sym or args.all:
        import difflib
        hits = 0
        for sz, k, src, path, bm, om in sorted(same, key=lambda r: (r[2], r[1])):
            if args.sym and k != args.sym:
                continue
            hits += 1
            print("======== %s  (%d bytes, %s)" % (k, sz, src))
            print("  tag: %s -- %s\n" % classify(bm, om))
            _, bt = disasm(BLOB, k)
            _, ot = disasm(path, k)
            # Addresses are dropped by disasm(), but a branch TARGET is an
            # operand and the two objects are at different addresses, so every
            # jump would read as a difference.  Blank the target.
            def norm(ls):
                return [re.sub(r"\b[0-9a-f]{2,8} <", "<", l) for l in ls]
            for l in difflib.unified_diff(norm(bt), norm(ot), "blob", "ours",
                                          lineterm="", n=2):
                print("  " + l)
            print()
        if not hits:
            sys.exit("samesize: %s is not in the same-size set" % args.sym)
        print("(%d symbols compared, %d printed)" % (len(common), hits))
        return 0

    print("samesize: %d symbols compared, %d identical, %d same size and "
          "different\n           TC_OUT=%s" % (len(common), len(ident),
                                               len(same), OURS))
    print("\n%-46s %6s %-13s %s" % ("SYMBOL", "BYTES", "TAG", "SOURCE / OWNER"))
    for sz, k, src, path, bm, om in sorted(same, key=lambda r: (r[2], r[1])):
        tag, why = classify(bm, om)
        who = owner(src, k)
        print("%-46s %6d %-13s %s%s"
              % (k[:46], sz, tag, src, "   [%s]" % who if who else ""))
        print("%54s %s" % ("", why))
    tags = Counter(classify(bm, om)[0] for _, _, _, _, bm, om in same)
    print("\nby tag: %s" % ", ".join("%s %d" % kv for kv in sorted(tags.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
