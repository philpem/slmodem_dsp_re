#!/usr/bin/env python3
"""gccdiverge.py -- run one test, and excuse ONLY the checks a modern compiler
provably cannot get right.

    tools/gccdiverge.py t_resampler build/test/t_resampler
    tools/gccdiverge.py --list

WHAT THIS IS FOR, and it is a narrow thing.

The reconstruction is compared against an object built by GCC 3.4.2.  At a few
sites the modern compiler CANNOT reproduce that object from correct source --
not because our source is wrong, but because the two compilers keep different
intermediate precision.  `round32` in src/pump/v90/Resampler.cpp is the worked
example (finding 1352): GCC 3.4.2 ran out of x87 registers and spilled an
accumulator, which rounded it to `float`; GCC 13 keeps 80 bits; and the
difference reaches the stored sample.

Before `make period` existed, the only way to keep the gate green was to force
the store from the SOURCE -- a `volatile` that exists for the compiler and not
for the object.  That is backwards: it makes the reconstruction less like what
the author wrote in order to satisfy a compiler the author never used.

So the source gets corrected, `make period` proves it against the object with
the period compiler, and the modern build declares the site here.

THE DISCIPLINE, because an allow-list is otherwise a place where failures go
to be forgotten:

  * IT NAMES CHECKS, NOT TESTS.  An entry excuses the listed check names and
    nothing else.  The same binary failing anything unlisted still fails.
  * A STALE ENTRY IS AN ERROR.  If an allow-listed test passes, this exits
    non-zero and says so.  Otherwise the register silently accumulates
    excuses for problems that fixed themselves, and the next real regression
    hides behind one.
  * IT APPLIES TO THE MODERN BUILD ONLY.  tools/toolchain/period_inner.sh
    never consults this file.  `make period` has no allow-list and is not
    getting one: the period compiler has no excuse, because it is the
    compiler the object was built with.
  * EVERY ENTRY CITES A FINDING.  "GCC 13 differs here" is not a reason; the
    finding is where the disassembly, the instruction sequence and the
    measurement live.

An entry is a statement that MODERN GCC IS WRONG AND WE KNOW WHY.  It is not
a tolerance, and it is not for a test that is merely inconvenient.
"""

import argparse
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REGISTER = os.path.join(HERE, "gccdiverge.json")

FAILLINE = re.compile(r"^\s*FAIL\s+(\S.*?)\s{2,}", re.M)


def load():
    try:
        with open(REGISTER) as f:
            return json.load(f)
    except OSError:
        return {}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("test", nargs="?")
    ap.add_argument("binary", nargs="?")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    reg = load()

    if args.list:
        if not reg:
            print("gccdiverge: no entries -- the modern build matches the "
                  "object everywhere it is tested")
            return 0
        for name, e in sorted(reg.items()):
            print("%s  (finding %s)" % (name, e.get("finding", "?")))
            for c in e.get("checks", []):
                print("    %s" % c)
            print("    %s" % e.get("why", "").strip())
        return 0

    if not args.test or not args.binary:
        ap.error("need a test name and a binary")

    r = subprocess.run([args.binary], capture_output=True, text=True)
    out = r.stdout + r.stderr
    entry = reg.get(args.test)

    if r.returncode == 0:
        sys.stdout.write(out)
        if entry:
            # Good news the gate must not swallow.  Either the compiler
            # improved or the reason expired; either way the register is now
            # lying, and a lying register is how the next real failure hides.
            print("  STALE  %s is allow-listed in tools/gccdiverge.json and "
                  "now PASSES." % args.test)
            print("         Delete the entry.  If the reason expired, say so "
                  "in finding %s." % entry.get("finding", "?"))
            return 1
        return 0

    sys.stdout.write(out)
    if not entry:
        return r.returncode

    failed = set(FAILLINE.findall(out))
    allowed = set(entry.get("checks", []))
    unexpected = failed - allowed

    if unexpected:
        print("  %s is allow-listed, but these checks are NOT covered:"
              % args.test)
        for c in sorted(unexpected):
            print("      %s" % c)
        print("  An entry excuses the checks it names and nothing else.")
        return r.returncode

    print("  ALLOWED  %s: %d check(s) differ under this compiler and cannot "
          "match the" % (args.test, len(failed)))
    print("           object -- %s" % entry.get("why", "").strip())
    print("           `make period` is authoritative here.  Finding %s."
          % entry.get("finding", "?"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
