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
                            compares go the long way with fnstsw/sahf
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

import glob
import os
import re
import subprocess
import sys

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


if __name__ == "__main__":
    main()
