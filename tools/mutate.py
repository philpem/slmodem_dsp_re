#!/usr/bin/env python3
"""
Break the code on purpose, and check the tests notice.

WHY THIS EXISTS

Task #50 restores ~220 diagnostic call sites by hand, from disassembly.  Every
one is a claim -- this string, these arguments, in this position -- and the
tests that check them are transcript comparisons written by the same hand, at
the same sitting, from the same reading.  A green run says the two agree.  It
does not say either is right.

Twice now the only thing that caught an error was deliberately introducing one
(findings 146, 149).  The second time it found something better than a bug: it
found that the test could not see call-site ORDER at all, because the callback
it was ordered against printed nothing.  Five mutations were caught and the
sixth was not, and the sixth was the interesting one.

So this is not a nice-to-have.  For a call site, "the test passes" and "the
test would have caught this being wrong" are different claims, and only the
second is worth writing down.

USAGE

    tools/mutate.py src/call/pulse.c build/test/t_pulse mutations.json

where mutations.json is a list of objects:

    [{"label": "swap the two arguments",
      "find":  "...exact text, must appear exactly once...",
      "replace": "..."}]

Each mutation is applied alone, the target rebuilt, the test run, and the
source restored -- in a `finally`, so an exception cannot leave a mutated tree
behind.  Exit status is non-zero if any mutation went UNCAUGHT, which is the
result worth failing on: an uncaught mutation is an untested claim.

WHAT A "CAUGHT" RESULT MEANS

Only that some check failed.  It does not mean the RIGHT check failed, and a
mutation caught by an unrelated assertion is weak evidence.  Pass --verbose to
see which checks fired.

A mutation that will not compile is reported separately from one that compiles
and passes: the first tells you nothing about the tests.
"""

import argparse
import json
import subprocess
import sys


def build_and_run(target, test):
    b = subprocess.run(["make", target], capture_output=True, text=True)
    if b.returncode != 0:
        return None, b.stderr
    r = subprocess.run([test], capture_output=True, text=True)
    return r.returncode, r.stdout


def main():
    ap = argparse.ArgumentParser(
        description="Apply mutations one at a time and report which the "
                    "tests catch.")
    ap.add_argument("source")
    ap.add_argument("test", help="test binary, e.g. build/test/t_pulse")
    ap.add_argument("mutations", help="JSON list of {label, find, replace}")
    ap.add_argument("--verbose", action="store_true",
                    help="show the failing check for each caught mutation")
    args = ap.parse_args()

    muts = json.load(open(args.mutations))
    good = open(args.source).read()
    target = args.test

    # Everything below runs against a mutated tree; the restore has to happen
    # even if the build dies or the user interrupts.
    uncaught, broken = [], []
    try:
        rc, out = build_and_run(target, args.test)
        if rc != 0:
            sys.exit("baseline is not green -- fix that first:\n" +
                     (out or ""))

        for m in muts:
            n = good.count(m["find"])
            if n != 1:
                broken.append((m["label"], "matches %d times, need 1" % n))
                print("  ????  %-52s  ANCHOR MATCHES %d TIMES"
                      % (m["label"], n))
                continue

            open(args.source, "w").write(
                good.replace(m["find"], m["replace"]))
            rc, out = build_and_run(target, args.test)

            if rc is None:
                broken.append((m["label"], "does not compile"))
                print("  ----  %-52s  did not compile" % m["label"])
                continue
            if rc == 0:
                uncaught.append(m["label"])
                print("  ****  %-52s  NOT CAUGHT" % m["label"])
                continue

            fails = [l for l in out.split("\n") if l.startswith("FAIL")]
            print("  ok    %-52s  caught" % m["label"])
            if args.verbose:
                for f in fails:
                    print("            %s" % f)
    finally:
        open(args.source, "w").write(good)
        build_and_run(target, args.test)

    print("\n  %d mutations: %d caught, %d NOT caught, %d unusable"
          % (len(muts), len(muts) - len(uncaught) - len(broken),
             len(uncaught), len(broken)))
    if uncaught:
        print("\n  Uncaught -- these claims are currently untested:")
        for l in uncaught:
            print("    %s" % l)
    return 1 if uncaught else 0


if __name__ == "__main__":
    sys.exit(main())
