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

    tools/mutate.py --suite v8jm            one set
    tools/mutate.py --all                   every set, with the totals
    tools/mutate.py src/call/pulse.c build/test/t_pulse mutations.json

PREFER --suite.  test/mutations/suites.json says which source each set
belongs to and which binary is meant to catch it, because getting that pairing
wrong produces NOT CAUGHT for every mutation in the set -- the same output an
untested claim gives, and indistinguishable from it without looking.  Six sets
were misread that way before the manifest existed.

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
import re
import subprocess
import sys


def build_and_run(target, test):
    b = subprocess.run(["make", target], capture_output=True, text=True)
    if b.returncode != 0:
        return None, b.stderr, None
    r = subprocess.run([test], capture_output=True, text=True)
    if r.returncode != 0:
        return r.returncode, r.stdout, "test"
    #
    # `make test` gates on `strings` as well as on the binaries, so a mutation
    # the invented-string sweep rejects is one the tests do catch -- and it is
    # the ONLY thing that catches a corrupted format string, because a wrong
    # string and a right one behave identically until someone raises the debug
    # level.  Running the binary alone reported fourteen of callprog.json's
    # seventeen as uncaught when the tree does in fact reject every one.
    #
    # WHICH GATE FIRED IS REPORTED SEPARATELY, because they are not the same
    # strength of evidence.  The differential test says the site prints this,
    # here, with these arguments.  The sweep says only that the literal exists
    # in the blob -- nothing about placement, arguments, or whether the site is
    # reachable at all.  A set caught entirely by `strings` is a set whose call
    # sites are still undriven, which is the thing task #50 exists to fix.
    #
    s = subprocess.run(["make", "strings"], capture_output=True, text=True)
    if s.returncode != 0:
        return s.returncode, s.stdout + s.stderr, "strings"
    return r.returncode, r.stdout, None


SUITES = "test/mutations/suites.json"


def run_all():
    """Every suite, with the totals -- so one command says where the tree is."""
    suites = {k: v for k, v in json.load(open(SUITES)).items() if k != "_"}
    tot = [0, 0, 0, 0, 0]
    for name in sorted(suites):
        r = subprocess.run([sys.executable, sys.argv[0], "--suite", name],
                           capture_output=True, text=True)
        last = [l for l in r.stdout.split("\n") if "mutations:" in l]
        print("  %-16s %s" % (name, last[-1].strip() if last else "FAILED"))
        if last:
            n = [int(x) for x in re.findall(r"(\d+) (?:caught|by test|"
                                            r"by strings|NOT caught|unusable)",
                                            last[-1])]
            for i, v in enumerate(n[:5]):
                tot[i] += v
    print("\n  %d caught -- %d by the differential tests, %d only by the "
          "string sweep\n  %d NOT caught, %d unusable, over %d suites"
          % (tot[0], tot[1], tot[2], tot[3], tot[4], len(suites)))
    return 1 if tot[3] or tot[4] else 0


def main():
    ap = argparse.ArgumentParser(
        description="Apply mutations one at a time and report which the "
                    "tests catch.")
    ap.add_argument("source", nargs="?")
    ap.add_argument("test", nargs="?",
                    help="test binary, e.g. build/test/t_pulse")
    ap.add_argument("mutations", nargs="?",
                    help="JSON list of {label, find, replace}")
    ap.add_argument("--suite", metavar="NAME",
                    help="a set named in test/mutations/suites.json, which "
                         "supplies its source and its test binary")
    ap.add_argument("--all", action="store_true",
                    help="every suite in test/mutations/suites.json")
    ap.add_argument("--verbose", action="store_true",
                    help="show the failing check for each caught mutation")
    args = ap.parse_args()

    if args.all:
        return run_all()
    if args.suite:
        suites = json.load(open(SUITES))
        if args.suite not in suites:
            sys.exit("no such suite: %s (have %s)"
                     % (args.suite, ", ".join(sorted(k for k in suites
                                                     if k != "_"))))
        args.source, args.test = suites[args.suite]
        args.mutations = "test/mutations/%s.json" % args.suite

    if not (args.source and args.test and args.mutations):
        sys.exit("give --suite NAME, --all, or source, test and mutations")

    muts = json.load(open(args.mutations))
    good = open(args.source).read()
    target = args.test

    # Everything below runs against a mutated tree; the restore has to happen
    # even if the build dies or the user interrupts.
    uncaught, broken = [], []
    by = {"test": 0, "strings": 0}
    try:
        rc, out, _ = build_and_run(target, args.test)
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
            rc, out, gate = build_and_run(target, args.test)

            if rc is None:
                broken.append((m["label"], "does not compile"))
                print("  ----  %-52s  did not compile" % m["label"])
                continue
            if rc == 0:
                uncaught.append(m["label"])
                print("  ****  %-52s  NOT CAUGHT" % m["label"])
                continue

            by[gate] += 1
            fails = [l for l in out.split("\n") if l.startswith("FAIL")]
            print("  ok    %-52s  caught (%s)" % (m["label"], gate))
            if args.verbose:
                for f in fails:
                    print("            %s" % f)
    finally:
        open(args.source, "w").write(good)
        build_and_run(target, args.test)

    print("\n  %d mutations: %d caught (%d by test, %d by strings), "
          "%d NOT caught, %d unusable"
          % (len(muts), len(muts) - len(uncaught) - len(broken),
             by["test"], by["strings"], len(uncaught), len(broken)))
    if uncaught:
        print("\n  Uncaught -- these claims are currently untested:")
        for l in uncaught:
            print("    %s" % l)
    return 1 if uncaught else 0


if __name__ == "__main__":
    sys.exit(main())
