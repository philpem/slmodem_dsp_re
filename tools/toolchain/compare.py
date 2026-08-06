#!/usr/bin/env python3
"""Compare our object code against the blob's, function by function.

The differential tier proves the two BEHAVE the same.  This asks the harder
question: does the same compiler, given our source, emit what the original
compiler emitted?  A function that comes out byte-identical is evidence about
the SOURCE that no amount of black-box testing can give -- it says the
expression shape, the operand order and the control flow were recovered, not
merely something equivalent to them.

Build the objects first, with the period toolchain (see the Dockerfile here and
finding 346):

    docker build --platform linux/386 -t dsplibs-tc tools/toolchain
    tools/toolchain/build.sh            # writes /tmp/tc_out/*.o
    tools/toolchain/compare.py

THE FLAGS ARE NOT GUESSES.  Each was read out of the object:

    -march=i386             no cmov and no fcomi anywhere in 1.2 MB, and float
                            compares go the long way with fnstsw/sahf.  This
                            bounds the INSTRUCTION SET only
    -mtune=i686             ...and the SCHEDULING is separately i686, which the
                            object cannot show directly but the match rate can:
                            i686 tuning takes byte-identical functions from 30
                            to 82.  Every i686-family tune value (pentiumpro,
                            pentium2, pentium3, and the period spelling
                            `-mcpu=i686`) gives the identical result
    -mfpmath=387            follows from the above
    -fomit-frame-pointer    no push %ebp / mov %esp,%ebp in the object's
                            prologues.  GCC 3.4 does NOT imply this at -O2
    -maccumulate-outgoing-args
                            the object pre-allocates its outgoing argument area
                            and fills it with `mov %reg,(%esp)`; without the
                            flag GCC pushes
    (no -fPIC/-fPIE)        not one get_pc_thunk in the object
    (no -fstack-protector)  no __guard, no __stack_smash_handler

`-O2` is the one assumption left, and it is the only level that fits what the
first four settled.  If a future round finds a systematic mismatch that a
different level explains, this comment is the place to record it.

WHAT THE NUMBERS MEAN, and do not mean.  A size mismatch is not a defect: our
source is not the original's source, and a function we wrote as one loop that
the original wrote as two will differ while behaving identically.  Read this as
a similarity gradient, not a pass/fail.  The differential tests remain the only
thing that decides correctness.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys

#
# THE RATCHET, and why it is a ratchet and not a gate.
#
# 100% is not the target and never will be: our source is not the original's
# source, so a function we wrote as one loop where the author wrote two will
# differ for ever while behaving identically.  A "must match" gate would fail
# on every file in the tree and teach everyone to ignore it.
#
# What IS meaningful is the direction.  A number that only ever goes up turns
# the comparison into a progress metric and, more usefully, catches the case
# where a change makes the reconstruction LESS like the original while all the
# differential tests still pass -- which is exactly the kind of regression this
# project has no other way to see.
#
RATCHET = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "ratchet.json")

#
# TRANSLATION UNITS WE DELIBERATELY SPLIT, and the original did not.
#
# A per-symbol size comparison across an inlining boundary measures the
# reconstruction's FACTORING, not its completeness -- the same trap
# debugaudit.py fell into (finding 345).  Where we broke one of the original's
# functions into static helpers, the helpers have no blob symbol, so their
# bytes are counted against neither side and the blob's function shows the
# whole difference as missing.  The per-object rollup below fixes that case on
# its own.
#
# It cannot fix a split across FILES, because there is nothing in either object
# to say the two belong together.  Those are declared here.  Finding 350.
#
TU_GROUPS = (
    ("src/v8/v8handshak.c", "src/v8/v8hsrx.c"),
)

BLOB = os.environ.get("BLOB", "../slmodemd/dsplibs.o")
OURS = os.environ.get("TC_OUT", "/tmp/tc_out")


def sizes(path):
    """symbol -> size, for anything with code in it."""
    out = subprocess.run(["nm", "--size-sort", "-S", path],
                         capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "TtWw":
            d[p[3]] = int(p[1], 16)
    return d


def mnemonics(path, sym):
    """The instruction mnemonic sequence, with addresses and operands dropped.

    Operands are dropped on purpose: a relocated address, a stack offset that
    differs by one slot and a register the allocator chose differently are all
    noise against the question being asked, which is whether the same
    instructions come out in the same order.
    """
    out = subprocess.run(
        ["objdump", "-d", "--disassemble=" + sym, "--no-show-raw-insn", path],
        capture_output=True, text=True).stdout
    return [m.group(1) for m in
            (re.match(r"^\s*[0-9a-f]+:\s+(\S+)", l) for l in out.splitlines())
            if m]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ratchet", action="store_true",
                    help="fail if fewer functions match than last time")
    ap.add_argument("--update", action="store_true",
                    help="record the current counts as the new floor")
    args = ap.parse_args()

    blob = sizes(BLOB)
    ours = {}
    for o in sorted(glob.glob(os.path.join(OURS, "*.o"))):
        for k, v in sizes(o).items():
            ours.setdefault(k, (v, o))
    if not ours:
        sys.exit("no objects in %s -- run tools/toolchain/build.sh first" % OURS)

    common = sorted(k for k in ours if k in blob)
    identical, samesize, rows = [], [], []
    tb = to = 0
    for k in common:
        b = blob[k]
        o, path = ours[k]
        tb += b
        to += o
        if b == o:
            if mnemonics(BLOB, k) == mnemonics(path, k):
                identical.append((b, k))
            else:
                samesize.append((b, k))
        rows.append((o - b, b, o, k))

    print("Comparing %d symbols the blob and this tree both have.\n" % len(common))
    print("  byte-for-byte identical instruction sequences : %4d" % len(identical))
    print("  same size, different instructions             : %4d" % len(samesize))
    print("  different size                                : %4d"
          % (len(common) - len(identical) - len(samesize)))
    print("\n  total code: blob %d bytes, ours %d (%.1f%%)"
          % (tb, to, 100.0 * to / tb if tb else 0))

    if identical:
        print("\nIDENTICAL (the reconstruction reproduces the original's codegen):")
        for b, k in sorted(identical, reverse=True):
            print("  %5d  %s" % (b, k))

    rows.sort()
    print("\nWhere we emit the least against the blob -- usually a helper the"
          "\noriginal inlined, or diagnostic sites we have not restored:")
    for delta, b, o, k in rows[:8]:
        print("  %+7d  blob %5d  ours %5d  %s" % (delta, b, o, k))
    print("\nWhere we emit the most:")
    for delta, b, o, k in rows[-5:]:
        print("  %+7d  blob %5d  ours %5d  %s" % (delta, b, o, k))

    #
    # PER OBJECT, which is the number to read before treating any single-symbol
    # gap as missing code.
    #
    def obj_text(path):
        """`.text` PLUS the linkonce sections -- the weak class templates put
        every member in one of those and would otherwise measure as zero."""
        out = subprocess.run(["size", "-A", path], capture_output=True,
                             text=True).stdout
        n = 0
        for line in out.splitlines():
            f = line.split()
            if len(f) >= 2 and (f[0] == ".text"
                                or f[0].startswith(".gnu.linkonce.t.")):
                n += int(f[1])
        return n

    # build.sh records object -> source; the underscore encoding is not
    # reversible (`dp_wrapper.c` would come back as `dp/wrapper.c`).
    manifest = {}
    try:
        for line in open(os.path.join(OURS, "..", "tc_manifest.txt")):
            o, s = line.split()
            manifest[o] = s
    except OSError:
        pass

    def srcname(path):
        b = os.path.basename(path)
        return manifest.get(b, b[:-2].replace("_", "/"))

    # Every object, not only those with a symbol in common: a file we split out
    # of one of the original's functions has NO symbol the blob shares, so
    # keying on `common` would drop exactly the file that explains the gap.
    per = {}
    for path in sorted(glob.glob(os.path.join(OURS, "*.o"))):
        b = sum(blob[k] for k in sizes(path) if k in blob)
        per[srcname(path)] = [b, obj_text(path)]
    for group in TU_GROUPS:
        present = [g for g in group if g in per]
        if len(present) > 1:
            b = sum(per[g][0] for g in present)
            o = sum(per[g][1] for g in present)
            for g in present[1:]:
                del per[g]
            per[present[0] + " (+%d split out)" % (len(present) - 1)] = [b, o]
            del per[present[0]]

    rank = sorted(((100.0 * o / b, b, o, s) for s, (b, o) in per.items() if b))
    print("\nPER OBJECT -- a helper the original inlined cannot hide here."
          "\nRead this before treating a single-symbol gap as missing code:")
    for pct, b, o, s in rank[:8]:
        print("  ours %5.0f%% of the blob   blob %6d  ours %6d  %s"
              % (pct, b, o, s))

    now = {"identical": len(identical), "same_size": len(samesize),
           "compared": len(common)}
    if args.update:
        with open(RATCHET, "w") as f:
            json.dump(now, f, indent=2, sort_keys=True)
            f.write("\n")
        print("\nratchet updated: %s" % now)
        return 0
    if args.ratchet:
        try:
            was = json.load(open(RATCHET))
        except (OSError, ValueError):
            sys.exit("no %s -- run with --update to set the floor" % RATCHET)
        bad = [k for k in ("identical", "same_size") if now[k] < was[k]]
        if bad:
            print("\nRATCHET FAILED -- the reconstruction moved AWAY from the"
                  "\noriginal's code generation, and no differential test can"
                  "\nsee that:")
            for k in bad:
                print("    %-10s was %d, now %d" % (k, was[k], now[k]))
            print("\n  If the change was deliberate, re-bless with --update"
                  "\n  and say in the commit message why fewer functions match.")
            return 1
        gained = [k for k in now if k in was and now[k] > was[k]]
        print("\nratchet OK%s" % ("" if not gained else
              " -- gained: " + ", ".join("%s %d->%d" % (k, was[k], now[k])
                                         for k in sorted(gained))))
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
