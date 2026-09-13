#!/usr/bin/env python3
"""Report defined symbols whose LOCAL/GLOBAL binding differs between two objects.

The partial-link comparator counts exact symbol RECORDS, and a record carries
the value as well as the binding, so a symbol whose binding is now correct but
whose offset still differs is not visible there.  This tool isolates the one
dimension the binding work is about: of the names both objects define, how many
carry the reference's linkage, and which do not.

    bindcmp.py ref/slmodemd/dsplibs.o build/partial/dsplibs.o

Every printed line carries its denominator.  It refuses to run on an object
with no defined symbols, so a wrong path is an error and not a clean report.
"""

import argparse
import subprocess
import sys


def defined(path):
    """name -> (kind, LOCAL|GLOBAL|WEAK) for each name the object defines."""
    out = subprocess.run(["nm", "--defined-only", path], capture_output=True,
                         text=True)
    if out.returncode != 0:
        sys.exit("error: nm failed on %s:\n%s" % (path, out.stderr))
    table = {}
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        kind, name = parts[-2], parts[-1]
        if kind in "tdrb":
            binding = "LOCAL"
        elif kind in "TDRBC":
            # Upper case is GLOBAL; `C` is a common symbol, which is also a
            # global definition (and what a tentative array becomes).
            binding = "GLOBAL"
        elif kind in "wv":
            binding = "WEAK"
        else:
            continue
        # One name normalised to its strongest binding if it appears twice.
        if name not in table or binding != "LOCAL":
            table[name] = binding
    return table


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("reference")
    ap.add_argument("candidate")
    args = ap.parse_args()

    ref = defined(args.reference)
    cand = defined(args.candidate)
    if not ref or not cand:
        sys.exit("error: refusing to report on an empty symbol table "
                 "(reference %d, candidate %d)" % (len(ref), len(cand)))

    shared = sorted(set(ref) & set(cand))
    mismatch = [(n, ref[n], cand[n]) for n in shared if ref[n] != cand[n]]
    agree = len(shared) - len(mismatch)
    print("defined-symbol binding: %d/%d shared names agree; %d differ"
          % (agree, len(shared), len(mismatch)))
    print("  reference defines %d names, candidate %d; %d reference-only, "
          "%d candidate-only"
          % (len(ref), len(cand), len(set(ref) - set(cand)),
             len(set(cand) - set(ref))))
    for name, r, c in mismatch:
        print("  %-40s reference %-6s candidate %-6s" % (name, r, c))
    return 1 if mismatch else 0


if __name__ == "__main__":
    sys.exit(main())
