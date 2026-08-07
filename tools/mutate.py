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

TWO OTHER KINDS OF ENTRY

An object with no "find" is a NOTE: prose kept in the set, printed and
otherwise ignored.  JSON has no comments and these files are arguments, not
data.

An object with "equivalent": true is a mutation that is EXPECTED TO SURVIVE,
because the change provably cannot alter behaviour -- a reordering of two
stores that do not alias, a mask bit that is OR-ed straight back in.  Such a
mutation surviving is the recorded result and does not fail the run.

They are here rather than deleted because "we tried this and it survived for a
reason" is worth more than the absence of an entry, which reads as "nobody
thought of it".  The reason goes in "why", and it has to be an argument about
the code -- not "the test does not cover it", which is the opposite result and
belongs in the uncaught list.

An equivalent mutation that gets CAUGHT is reported and fails the run: either
the reasoning was wrong or the code moved under it, and both want looking at.

WHAT A "CAUGHT" RESULT MEANS

Only that some check failed.  It does not mean the RIGHT check failed, and a
mutation caught by an unrelated assertion is weak evidence.  Pass --verbose to
see which checks fired.

A mutation that will not compile is reported separately from one that compiles
and passes: the first tells you nothing about the tests.
"""

import argparse
import json
import os
import re
import signal
import subprocess
import sys

#
# OPERATE ON THIS SCRIPT'S OWN TREE, whatever the caller's directory is.
#
# Every path below is relative, `make` inherits the cwd, and the restore in
# the `finally` writes back to a relative path too.  Several worktrees of this
# repository exist side by side and are edited at the same time; run from the
# wrong one and this mutates another tree's source and rebuilds another tree's
# objects, with the restore landing there as well.  Nothing about the output
# would say so.
#
os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


#
# A MUTATION CAN HANG THE BINARY, and until one did there was no timeout here.
#
# `probe_preemph`'s counter is advanced before the test that leaves its loop.
# Mutating the advance to below that test -- which is exactly the claim worth
# breaking -- produces a loop with no exit, and the sweep then sat on one
# `t_v34hshak` for sixty-two minutes with no output, because mutate.py prints
# nothing until it finishes.  From the outside that is indistinguishable from
# a slow build.
#
# So: every test run is bounded.  A mutation that times out is reported as
# CAUGHT, and separately as a hang -- it IS caught, in the only sense that
# matters (the tree does not silently accept it), but "the test failed" and
# "the test never returned" are different results and a set full of the
# second is a set worth looking at.
#
# The bound is per test binary and generous: the slowest in this tree is a
# little over two seconds, and `t_v34hshak`'s constellation sweep alone is
# fourteen million checks.  Finding 200 is the same lesson from the other
# side -- 88% of a sweep spent inside one timeout nobody had noticed.
#
RUN_TIMEOUT = 120


#
# AND THE RESTORE HAS TO SURVIVE BEING KILLED.
#
# The source is restored in a `finally`, which covers an exception and a
# Ctrl-C -- SIGINT raises KeyboardInterrupt -- and does NOT cover SIGTERM,
# which terminates the interpreter outright.  So `kill` on a sweep that is
# stuck leaves the tree carrying whichever mutation was live, and the next
# thing anyone builds is a mutant.  That cost an hour: `t_v34hshak` hung, a
# clean rebuild hung the same way, and the source looked right because the
# mutation was forty lines from the function being examined.
#
# Turning SIGTERM into an exception is enough -- `finally` then runs and the
# tree is clean whichever way the sweep ends.
#
def _term(signum, frame):
    raise KeyboardInterrupt("killed by signal %d" % signum)


signal.signal(signal.SIGTERM, _term)


def build_and_run(target, test):
    b = subprocess.run(["make", target], capture_output=True, text=True)
    if b.returncode != 0:
        return None, b.stderr, None
    try:
        r = subprocess.run([test], capture_output=True, text=True,
                           timeout=RUN_TIMEOUT)
    except subprocess.TimeoutExpired:
        return 1, "TIMED OUT after %ds" % RUN_TIMEOUT, "hang"
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

#
# RUNNING A SUITE IN PARALLEL, AND WHY IT NEEDS A TREE PER WORKER
#
# A mutation is: write the mutant over the source, build, run, put the source
# back.  That is inherently serial IN ONE TREE -- two workers would be writing
# the same file, which is finding 349's accident on purpose -- and it is why a
# 749-mutation suite takes a quarter of an hour on a twelve-core machine that
# is idle for all of it.  Measured: 0.67 s to rebuild and relink one
# translation unit, 0.73 s to run the test.
#
# Mutations are INDEPENDENT of one another, so the fix is a tree per worker
# rather than a lock.  The source tree without `.git` and the build
# directories is 7.8 MB and a build directory for one test binary is 7.5 MB,
# so eight workers cost about 120 MB of temporary disk and no cleverness.
#
# The workers are SIBLINGS of the real tree, not under /tmp, because
# `third_party/spandsp` is a RELATIVE symlink (`../../claude_re/...`) that
# only resolves at the same depth.  A worker under /tmp builds everything,
# then dies naming spandsp -- the same trap a fresh worktree has.
#
# A shard runs the ordinary serial path below; the classification is not
# duplicated or reimplemented, it just hands its four lists back as JSON.
# That is deliberate.  This is the tier that decides whether a claim is
# tested at all, and a second copy of "what counts as caught" is exactly the
# kind of thing that drifts apart from the first.
#
SHARD_MARK = "##SHARD##"
COPY_SKIP = (".git", "build*")


def report(total, uncaught, broken, equivalent, surprises, by):
    """The summary, in one place so a shard and a serial run cannot differ."""
    print("\n  %d mutations: %d caught (%d by test, %d by strings), "
          "%d NOT caught, %d unusable, %d equivalent, %d MIScounted"
          % (total,
             total - len(uncaught) - len(broken) - len(equivalent),
             by["test"], by["strings"], len(uncaught), len(broken),
             len(equivalent), len(surprises)))
    if by["hang"]:
        print("  %d of those never returned and were killed at %ds -- caught,"
              " but by the clock" % (by["hang"], RUN_TIMEOUT))
    if uncaught:
        print("\n  Uncaught -- these claims are currently untested:")
        for l in uncaught:
            print("    %s" % l)
    if equivalent:
        print("\n  Equivalent -- survived, and recorded as unable to fail:")
        for l, why in equivalent:
            print("    %s\n      %s" % (l, why))
    if surprises:
        print("\n  RECORDED AS EQUIVALENT AND CAUGHT ANYWAY -- the argument")
        print("  for these is wrong, or the code has moved under it:")
        for l in surprises:
            print("    %s" % l)
    return 1 if uncaught or surprises else 0


def shard_of(muts, spec):
    """CONTIGUOUS chunks, not round-robin, so merged output keeps its order."""
    i, n = (int(x) for x in spec.split("/"))
    per = (len(muts) + n - 1) // n
    return muts[i * per:(i + 1) * per]


#
# A WORKER HAS TO EARN ITS SETUP.
#
# Copying the tree and doing one cold build costs a second or two per worker.
# On a big suite that is nothing; on a small one it is the whole run, and
# MEASURED, --jobs 4 over `cadence`, `pulse` and `dilpack` (6, 9 and 8
# mutations) took 17.9 s against 11.2 s serial -- sharding made it 60%
# SLOWER.  That matters because `--all --jobs 8` is the command someone will
# reach for, and thirty of the forty-eight suites are that small.
#
# So the fan-out degrades itself: a worker gets at least this many mutations
# or it is not started, and one worker means the ordinary serial path.
#
MIN_PER_WORKER = 12


def run_parallel(args, jobs):
    """Fan the suite out over `jobs` sibling trees and merge the verdicts."""
    import shutil
    here = os.getcwd()
    root = os.path.dirname(here)
    stamp = "%s-%d" % (os.path.basename(here), os.getpid())
    trees, procs = [], []
    try:
        for i in range(jobs):
            d = os.path.join(root, ".mutshard-%s-%d" % (stamp, i))
            shutil.rmtree(d, ignore_errors=True)
            shutil.copytree(here, d, symlinks=True,
                            ignore=shutil.ignore_patterns(*COPY_SKIP))
            trees.append(d)
        cmd = [sys.executable, "tools/mutate.py"]
        cmd += (["--suite", args.suite] if args.suite
                else [args.source, args.test, args.mutations])
        for i, d in enumerate(trees):
            procs.append(subprocess.Popen(
                cmd + ["--shard", "%d/%d" % (i, jobs)], cwd=d,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True))
        uncaught, broken, equivalent, surprises = [], [], [], []
        by, total, failed = {"test": 0, "strings": 0, "hang": 0}, 0, []
        for i, p in enumerate(procs):
            out = p.communicate()[0]
            blob = [l for l in out.split("\n") if l.startswith(SHARD_MARK)]
            if not blob:
                #
                # A shard that dies is NOT a shard with nothing to report.
                # Losing one silently would drop an eighth of the suite and
                # still print a total, which is the failure mode findings 347
                # and 540 are both about.
                #
                failed.append((i, out[-2000:]))
                continue
            r = json.loads(blob[-1][len(SHARD_MARK):])
            print(r["text"], end="")
            uncaught += r["uncaught"]
            broken += [tuple(x) for x in r["broken"]]
            equivalent += [tuple(x) for x in r["equivalent"]]
            surprises += r["surprises"]
            total += r["total"]
            for k in by:
                by[k] += r["by"][k]
        if failed:
            for i, tail in failed:
                print("\n  SHARD %d DIED -- its mutations were NOT run:\n%s"
                      % (i, tail))
            print("\n  %d of %d shards died; this run is not a result."
                  % (len(failed), jobs))
            return 2
        return report(total, uncaught, broken, equivalent, surprises, by)
    finally:
        for d in trees:
            shutil.rmtree(d, ignore_errors=True)


def run_all(jobs=None):
    """Every suite, with the totals -- so one command says where the tree is."""
    suites = {k: v for k, v in json.load(open(SUITES)).items() if k != "_"}
    tot = [0, 0, 0, 0, 0, 0, 0]
    for name in sorted(suites):
        #
        # __file__, not argv[0].  The chdir above has already happened, so a
        # relative argv[0] -- `tools/mutate.py`, or `../other/tools/mutate.py`
        # from another worktree -- would be resolved against the NEW cwd and
        # either fail or, worse, find a different tree's copy.
        #
        r = subprocess.run([sys.executable, os.path.abspath(__file__),
                            "--suite", name]
                           + (["--jobs", str(jobs)] if jobs else []),
                           capture_output=True, text=True)
        last = [l for l in r.stdout.split("\n") if "mutations:" in l]
        print("  %-16s %s" % (name, last[-1].strip() if last else "FAILED"))
        if last:
            n = [int(x) for x in re.findall(r"(\d+) (?:caught|by test|"
                                            r"by strings|NOT caught|unusable|"
                                            r"equivalent|MIScounted)",
                                            last[-1])]
            for i, v in enumerate(n[:7]):
                tot[i] += v
    print("\n  %d caught -- %d by the differential tests, %d only by the "
          "string sweep\n  %d NOT caught, %d unusable, %d equivalent, "
          "over %d suites"
          % (tot[0], tot[1], tot[2], tot[3], tot[4], tot[5], len(suites)))
    #
    # tot[6] is the count recorded as equivalent and CAUGHT anyway.  It has
    # to be carried through the summary line and counted here, or the whole
    # point of the "equivalent" key is lost: a suite whose recorded argument
    # has gone stale is exactly the case the key exists to catch, and
    # without this it reads green under --all while --suite fails.
    #
    if tot[6]:
        print("  %d recorded as equivalent and caught anyway -- run the "
              "suite for which" % tot[6])
    return 1 if tot[3] or tot[4] or tot[6] else 0


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
    ap.add_argument("--jobs", type=int, metavar="N",
                    help="run the suite over N sibling trees at once; each "
                         "costs about 15 MB of temporary disk and the "
                         "verdicts are identical to a serial run")
    ap.add_argument("--shard", metavar="I/N",
                    help=argparse.SUPPRESS)   # set by --jobs on its workers
    args = ap.parse_args()

    if args.all:
        return run_all(args.jobs)
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

    if args.jobs and args.jobs > 1 and not args.shard:
        n = len([m for m in json.load(open(args.mutations)) if "find" in m])
        want = max(1, min(args.jobs, n // MIN_PER_WORKER))
        if want > 1:
            return run_parallel(args, want)

    entries = json.load(open(args.mutations))
    muts = [m for m in entries if "find" in m]
    if args.shard:
        muts = shard_of(muts, args.shard)
    else:
        for note in [m for m in entries if "find" not in m]:
            print("  note  %s" % note.get("note", ""))
    good = open(args.source).read()
    target = args.test

    #
    # A shard's per-mutation lines are held and handed to the parent rather
    # than printed here, so that eight workers finishing out of order still
    # produce one transcript in the suite's own order.
    #
    import contextlib
    import io
    held = io.StringIO()
    stdout = contextlib.redirect_stdout(held) if args.shard else None
    if stdout:
        stdout.__enter__()

    # Everything below runs against a mutated tree; the restore has to happen
    # even if the build dies or the user interrupts.
    uncaught, broken, equivalent, surprises = [], [], [], []
    by = {"test": 0, "strings": 0, "hang": 0}
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

            #
            # A mutation that does not change the file is not a mutation.
            # It rebuilds the unmutated source, passes, and reports either
            # NOT CAUGHT or -- if it carries "equivalent" -- `survived,
            # equivalent`, which is indistinguishable from the real thing
            # and keeps the totals steady while checking nothing.
            #
            # Not hypothetical: re-anchoring a suite after a refactor made
            # five of these in one pass (finding 572).  The old `replace`
            # was the bare store that the new factoring had turned into the
            # unmutated text, so `find` and `replace` came out equal.  All
            # five were `equivalent: true`, so the totals did not move and
            # nothing else in the harness could see it.
            #
            mutated = good.replace(m["find"], m["replace"])
            if mutated == good:
                broken.append((m["label"],
                               "replace leaves the source unchanged"))
                print("  ????  %-52s  VACUOUS -- REPLACE == FIND"
                      % m["label"])
                continue

            open(args.source, "w").write(mutated)
            rc, out, gate = build_and_run(target, args.test)

            if rc is None:
                broken.append((m["label"], "does not compile"))
                print("  ----  %-52s  did not compile" % m["label"])
                continue
            if rc == 0:
                if m.get("equivalent"):
                    equivalent.append((m["label"], m.get("why", "")))
                    print("  ==    %-52s  survived, equivalent"
                          % m["label"])
                else:
                    uncaught.append(m["label"])
                    print("  ****  %-52s  NOT CAUGHT" % m["label"])
                continue
            if m.get("equivalent"):
                #
                # Recorded as behaviour-preserving and the tests disagree.
                # The argument in "why" is about the code, so either it was
                # wrong or the code has moved out from under it.
                #
                surprises.append(m["label"])
                print("  !!    %-52s  CAUGHT, recorded as equivalent"
                      % m["label"])

            by[gate] += 1
            fails = [l for l in out.split("\n") if l.startswith("FAIL")]
            print("  ok    %-52s  caught (%s)" % (m["label"], gate))
            if args.verbose:
                for f in fails:
                    print("            %s" % f)
    finally:
        open(args.source, "w").write(good)
        build_and_run(target, args.test)
        if stdout:
            stdout.__exit__(None, None, None)

    if args.shard:
        print(SHARD_MARK + json.dumps({
            "text": held.getvalue(), "total": len(muts),
            "uncaught": uncaught, "broken": broken,
            "equivalent": equivalent, "surprises": surprises, "by": by}))
        return 0

    return report(len(muts), uncaught, broken, equivalent, surprises, by)


if __name__ == "__main__":
    sys.exit(main())
