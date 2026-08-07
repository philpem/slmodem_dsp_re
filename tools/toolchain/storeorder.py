#!/usr/bin/env python3
"""Where does our STATEMENT ORDER differ from the original's?

A DIFFERENCE HERE IS A HINT, NOT A CONCLUSION, and the distinction cost a
false premise once already (finding 617).  GCC does NOT simply preserve source
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
a list we sorted is describing our sorting.  Findings 615 and 617.

Only stores through a register holding a pointer argument are considered, and
only functions where the two SETS agree -- a different set is a different
question (missing or extra work), not a different order.

    tools/toolchain/storeorder.py [--all]

Without --all, only functions whose order differs are listed.
"""

import glob
import os
import re
import subprocess
import sys

BLOB = os.environ.get("BLOB", "../slmodemd/dsplibs.o")
OURS = os.environ.get("TC_OUT", "/tmp/tc_out")


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


def main():
    show_all = "--all" in sys.argv
    blob = sizes(BLOB)
    ours = {}
    for o in sorted(glob.glob(os.path.join(OURS, "*.o"))):
        for k, v in sizes(o).items():
            ours.setdefault(k, v)

    diff = same = 0
    for k in sorted(ours):
        if k not in blob:
            continue
        a, b = store_seq(BLOB, k), store_seq(ours[k][1], k)
        if len(a) < 2 or sorted(a) != sorted(b):
            continue
        if a == b:
            same += 1
            if show_all:
                print("  same   %s" % k)
            continue
        diff += 1
        ascending = b == sorted(b, key=lambda x: int(x, 16))
        print("  %-42s %s" % (k, "ours is SORTED BY OFFSET" if ascending else ""))
        print("      object: %s" % " ".join(a))
        print("      ours:   %s" % " ".join(b))
    print("\n  %d functions differ in store order, %d already agree" % (diff, same))


if __name__ == "__main__":
    sys.exit(main() or 0)
