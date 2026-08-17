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
source restored.  ALL OF THAT HAPPENS IN A COPY of the tree, under $TMPDIR:
this script never writes to the tree you are working in, so no way of killing
it can leave a deliberate defect in your source.  See "THE RUN HAPPENS IN A
COPY" below for what that costs and why a plain copy rather than hardlinks.

Exit status is non-zero if any mutation went UNCAUGHT, which is the result
worth failing on: an uncaught mutation is an untested claim.

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
import atexit
import glob
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile

#
# OPERATE ON THIS SCRIPT'S OWN TREE, whatever the caller's directory is.
#
# Every path below is relative and `make` inherits the cwd.  Several worktrees
# of this repository exist side by side and are edited at the same time; run
# from the wrong one and this reads another tree's source and reports on
# another tree's objects.  Nothing about the output would say so.
#
# Since the mutation itself now happens in a copy, this line decides which tree
# gets COPIED rather than which tree gets written -- a smaller stake than it
# used to be, and still the one that decides whose verdicts these are.
#
REAL_TREE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REAL_TREE)


#
# A MUTATION CAN HANG THE BINARY, and until one did there was no timeout here.
#
# `probe_preemp`'s counter is advanced before the test that leaves its loop.
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
# SIGTERM, AND WHAT IT IS STILL FOR.
#
# This handler was the tree's safety net: the source was restored in a
# `finally`, which covers an exception and a Ctrl-C -- SIGINT raises
# KeyboardInterrupt -- and does NOT cover SIGTERM, which terminates the
# interpreter outright, so `kill` on a stuck sweep left the tree carrying
# whichever mutation was live.  That cost an hour once, and the `finally` was
# never enough anyway, which is why the run now happens in a copy.
#
# It stays because it is now what makes the COPY get cleaned up: an `atexit`
# handler does not run on a SIGTERM either, and turning the signal into an
# exception gets both the restore and the `shutil.rmtree` run.  `kill` on a
# sweep leaves nothing behind; only SIGKILL leaves the directory.
#
def _term(signum, frame):
    raise KeyboardInterrupt("killed by signal %d" % signum)


signal.signal(signal.SIGTERM, _term)



#
# WHY THIS EXISTS.  `tools/gccdiverge.json` allow-lists checks that modern GCC
# provably cannot reproduce, and `make test` honours it -- but MUTATION TESTING
# CANNOT.  A mutant is judged caught by the binary exiting non-zero, so a
# binary whose baseline already exits non-zero for an ALLOWED reason cannot
# distinguish "the mutation was caught" from "this test was already red", and
# the whole suite is unusable rather than partly usable.
#
# That is a real limitation and not a bug to route around: the honest move is
# to refuse, which the baseline gate above already did.  What it did NOT do is
# say WHY, so a register entry looked like a broken suite and cost one batch a
# cycle working out that the two features interact at all.  Neither tool
# mentioned the other.  This says it.
#
def diverge_note(test, out):
    """If the baseline failed only on allow-listed checks, name the register."""
    try:
        with open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "gccdiverge.json"), encoding="utf-8") as fh:
            reg = json.load(fh)
    except OSError:
        return ""
    entry = reg.get(os.path.basename(test))
    if not entry:
        return ""
    hits = [c for c in entry.get("checks", ()) if c in out]
    if not hits:
        return ""
    return ("\n\n%s IS IN tools/gccdiverge.json, and that is why this baseline "
            "is red.\n"
            "Allow-listed check(s) failing here: %s  (finding %s)\n"
            "\n"
            "`make test` tolerates these; mutation testing CANNOT, because a\n"
            "mutant is judged caught by a non-zero exit and this binary already\n"
            "exits non-zero.  Caught and already-red are indistinguishable, so\n"
            "the suite is refused rather than scored wrongly.\n"
            "\n"
            "This is a limitation, not a defect to work around.  `make period`\n"
            "is the tier that has no allow-list and passes these checks; a\n"
            "mutation set for this binary has to be judged there, or the\n"
            "divergent check has to be split out of it."
            % (os.path.basename(test), ", ".join(hits), entry.get("finding", "?")))

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
# THE RUN HAPPENS IN A COPY.  THE TREE YOU ARE WORKING IN IS NEVER WRITTEN.
#
# Applying a mutation is: write the mutant over the source, build, run, put the
# source back.  The restore was a `finally`, which covers an exception and --
# with the handler above -- a SIGTERM, and does NOT cover SIGKILL, a session
# teardown, or the machine dying.  What survived one of those was a source file
# holding a deliberate defect whose label reads exactly like a plausible
# reconstruction error ("rxstate and txstate offsets transposed"), and since
# EVERY test binary links ALL of `src/` (mutsnap.py's CLOSURE note), ONE stray
# mutant makes EVERY suite's verdicts meaningless.  Three of those in one
# session cost two full re-record runs and looked convincingly like a
# concurrency bug.
#
# So the mutating half of this script copies what a build needs into a
# directory under $TMPDIR, chdirs into it, and does everything there.  A
# SIGKILL now leaves a stale temp directory, which is garbage -- a different
# kind of thing from a corrupted tree, and one no later run can be misled by.
#
# MEASURED on the tree this comment was written against, twelve cores:
#
#     cp -a Makefile src include test tools docs      20 ms    9.7 MB
#     + build/ minus the linked test binaries         30 ms     21 MB total
#     first `make` in the copy (a link, no compiles) 128 ms
#     ... where a cold build of the same target is   1.38 s
#
# so setting a copy up costs about 180 ms, and it REPLACES the 1.4 s cold build
# every shard used to do.  END TO END IT IS FASTER, not a tax: `cadence`,
# `pulse` and `dilpack` run back to back -- the pessimal case, three small
# suites where fixed costs are most of the time -- take
#
#     in place, as this file was          13.0 s   12.5 s
#     in a copy, as it is now              9.9 s   10.3 s
#
# because the 180 ms is more than paid for by dropping the restore-and-rebuild
# that used to follow the last mutation.  That rebuild existed to leave the
# REAL tree's binaries matching its source; a copy about to be deleted does not
# need it, and it was a full build-test-and-`make strings` cycle, ~1.4 s.
# A large suite amortises the 180 ms to nothing, so this is the worst case.
#
# A PLAIN COPY, NOT `cp -al`.  Hardlinks are the obvious optimisation -- 13 ms
# against 33 -- and they are wrong three ways.  `open(path, "w")` on a
# hardlinked file truncates THROUGH the link into the original, so the bug
# relocates rather than goes away: `os.replace` would fix this file's own
# writes and not the compiler's, an editor's, or any other writer's.  `cp -al`
# cannot cross filesystems, and /tmp being the same volume as the tree is an
# accident of this machine -- it is tmpfs on plenty of others.  And it buys
# 11 ms.
#
# `build/` IS COPIED AND MUST NOT BE SHARED: the compiler writes objects by
# truncation, so a shared build/ would corrupt the parent's while it was being
# read.  What is copied is every object; what is NOT copied is the 129 linked
# binaries directly under `build/test/`, which are 296 MB of the 309 MB and all
# but one of them useless to any one suite.  The one that is wanted gets linked
# in the copy, in the 128 ms above.
#
# THE LIST IS SIZED TO WHAT `build_and_run` ACTUALLY INVOKES, which is `make
# <the suite's test binary>` and `make strings` -- the latter runs
# `tools/debugaudit.py`, which is why `tools/` is here.  `docs/` is NOT: no
# tool either of those two targets runs opens it (coverage.py only NAMES a doc,
# in a comment).  A target added to `build_and_run` later that does read it
# will fail loudly and this list is where to fix it.
#
COPY = ("Makefile", "src", "include", "test", "tools")
WORKDIR_PREFIX = "mutate-"


def _cp(paths, dest):
    """`cp -a`, because 20 ms of it is not worth reimplementing in Python."""
    if paths:
        subprocess.run(["cp", "-a"] + list(paths) + [dest], check=True)


def reap_stale_workdirs():
    """Delete copies a SIGKILLed run left behind.  Only ones whose pid is gone.

    The dangerous direction does not exist: a running copy's owner is alive by
    definition, so `os.kill(pid, 0)` succeeds and the directory is skipped.  A
    pid that has been REUSED reads as alive, which errs towards keeping
    garbage.  PermissionError likewise means the process exists.
    """
    for d in glob.glob(os.path.join(tempfile.gettempdir(),
                                    WORKDIR_PREFIX + "*")):
        m = re.match(WORKDIR_PREFIX + r"(\d+)-", os.path.basename(d))
        if not m or not os.path.isdir(d):
            continue
        try:
            os.kill(int(m.group(1)), 0)
        except ProcessLookupError:
            shutil.rmtree(d, ignore_errors=True)
        except OSError:
            pass


def tree_relative(p, what):
    """Spell a path relative to the tree, so that it lands in the COPY.

    AN ABSOLUTE PATH SURVIVES THE CHDIR and goes on naming the real tree, which
    would restore the exact defect this file has just stopped having -- and a
    run doing it would look completely ordinary, which is the shape of every
    bug in `docs/method/gates.md`.  `--suite` reads relative paths out of
    suites.json and cannot hit this; the three-positional-argument form is
    where someone can type one.
    """
    rel = os.path.relpath(os.path.abspath(p), REAL_TREE)
    if rel == os.pardir or rel.startswith(os.pardir + os.sep):
        sys.exit("mutate.py: the %s is outside the tree, so it cannot be "
                 "mutated in a copy of it: %s" % (what, p))
    return rel


def enter_workdir():
    """Copy what a build needs into a temp dir, chdir into it, return its path.

    Everything after this point -- the mutation, the build, the test run, the
    restore -- happens inside the copy.
    """
    reap_stale_workdirs()
    #
    # $BLOB is resolved HERE, while the cwd is still the real tree, because
    # nothing about the copy's location can find it: it is under $TMPDIR, so
    # neither a relative default nor `git rev-parse` resolves from there.
    # Get it wrong and the copy compiles and links perfectly and then dies
    # inside `make strings`, which is the late and confusing place to find out.
    #
    # ASK MAKE, rather than repeating its default here.  This used to hard-code
    # a relative path, and the Makefile's default has now moved twice -- through
    # `git rev-parse --git-common-dir` for worktrees, and then to
    # `ref/slmodemd/dsplibs.o`.  Either move leaves a hard-coded copy pointing
    # at a path that does not exist, and from a worktree -- which is where every
    # agent works -- that meant NO mutation suite could be run at all.  Loud,
    # but total.  An explicit `BLOB=` in the environment still wins, because
    # make's `?=` gives it precedence and this asks make.  Finding 6403.
    #
    os.environ["BLOB"] = os.path.abspath(
        subprocess.run(["make", "-s", "print-BLOB"], capture_output=True,
                       text=True, check=True).stdout.strip())
    d = tempfile.mkdtemp(prefix=WORKDIR_PREFIX + "%d-" % os.getpid())
    #
    # Registered on the ABSOLUTE path and before the chdir, because atexit runs
    # after it.  Same reason the lock below is absolute.
    #
    atexit.register(shutil.rmtree, d, ignore_errors=True)
    _cp([p for p in COPY if os.path.exists(p)], d)
    if os.path.isdir("build"):
        os.makedirs(os.path.join(d, "build"))
        _cp([os.path.join("build", e) for e in sorted(os.listdir("build"))
             if e != "test"], os.path.join(d, "build"))
        bt = os.path.join("build", "test")
        if os.path.isdir(bt):
            os.makedirs(os.path.join(d, bt))
            _cp([os.path.join(bt, e) for e in sorted(os.listdir(bt))
                 if os.path.isdir(os.path.join(bt, e))], os.path.join(d, bt))
    #
    # `third_party/spandsp` is a real 36 MB directory in one tree and a symlink
    # to that in every worktree.  Neither is copied: the copy gets an ABSOLUTE
    # symlink to whatever it really is.  That is what lets a copy live under
    # /tmp at all -- the shard trees this replaced had to be SIBLINGS of the
    # real tree, because a RELATIVE symlink only resolves at the same depth and
    # a worker elsewhere built everything and then died naming spandsp.
    #
    sp = os.path.join("third_party", "spandsp")
    if os.path.exists(sp):
        os.makedirs(os.path.join(d, "third_party"), exist_ok=True)
        os.symlink(os.path.realpath(sp), os.path.join(d, sp))
    os.chdir(d)
    #
    # STDERR, not stdout.  Three separate parsers read a child's stdout here --
    # mutsnap.py's verdict regexes, run_parallel's SHARD_MARK, run_all's
    # "mutations:" -- and none of them should have to know about this line.
    #
    sys.stderr.write("mutate.py: working in %s\n" % d)
    return d


#
# RUNNING A SUITE IN PARALLEL
#
# A mutation is inherently serial IN ONE TREE -- two workers would be writing
# the same file, which is finding 349's accident on purpose -- and it is why a
# 749-mutation suite takes a quarter of an hour on a twelve-core machine that
# is idle for all of it.  Measured: 0.67 s to rebuild and relink one
# translation unit, 0.73 s to run the test.
#
# Mutations are INDEPENDENT of one another, so the fix is a tree per worker
# rather than a lock -- and now that EVERY run works in a copy, that is no
# longer a special case.  `--jobs N` starts N ordinary runs with `--shard i/N`
# and each makes its own copy the same way a serial run does; there is one
# piece of copy code and both paths go through it.
#
# A shard runs the ordinary serial path below; the classification is not
# duplicated or reimplemented, it just hands its four lists back as JSON.
# That is deliberate.  This is the tier that decides whether a claim is
# tested at all, and a second copy of "what counts as caught" is exactly the
# kind of thing that drifts apart from the first.
#
SHARD_MARK = "##SHARD##"


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
# A WORKER HAS TO EARN ITS SETUP -- AND THE PRICE HAS CHANGED.
#
# THE MEASUREMENT THIS CONSTANT WAS SET FROM IS STALE.  It is kept here
# because deleting it would only invite the next reader to re-derive it.  It
# was taken against a fan-out that `shutil.copytree`d the WHOLE tree per worker
# and then did a COLD BUILD in it -- a second or two each -- and it said:
# --jobs 4 over `cadence`, `pulse` and `dilpack` (6, 9 and 8 mutations) took
# 17.9 s against 11.2 s serial, so sharding three small suites was 60% SLOWER,
# and thirty of the suites are that small.  Hence a floor of 12.
#
# A WORKER NOW COPIES THE OBJECTS TOO, so its first `make` is a link and not a
# build: about 180 ms of setup where it used to be a second and a half.
# Re-measured, same three suites, same machine, twice each:
#
#     serial                                  12.0 s   11.7 s
#     --jobs 4, no floor at all                7.8 s    7.9 s
#     --jobs 4, floor of 2 (as shipped)        8.0 s    8.3 s
#     --jobs 4, floor of 4                    10.5 s   10.9 s
#     --jobs 4, floor of 12 (the old value)     = serial, by construction
#
# So the cliff this constant existed to correct is GONE: on the exact suites
# that motivated it, sharding is now a third FASTER rather than 60% slower, and
# every floor above 2 costs time rather than saving it.  A worker earns its
# setup at roughly ONE mutation, because what is left of the setup is the
# baseline build-and-run each shard has to do anyway.
#
# It is kept at 2 as a TRIVIALITY GUARD and not as a cliff correction: starting
# a process to run a single mutation is noise in the scheduler for no gain, and
# 2 measures the same as no floor at all.  Anyone raising it again has the
# table above to beat.
#
MIN_PER_WORKER = 2


def run_parallel(args, jobs):
    """Fan the suite out over `jobs` shards and merge the verdicts.

    Each shard is an ordinary run with `--shard i/N`; it makes its own copy
    through `enter_workdir` exactly as a serial run does, so there is no
    second copy path here to drift away from the first.
    """
    #
    # __file__ rather than a relative `tools/mutate.py`, for the reason
    # `run_all` gives below: the module-level chdir has already happened.
    #
    cmd = [sys.executable, os.path.abspath(__file__)]
    cmd += (["--suite", args.suite] if args.suite
            else [args.source, args.test, args.mutations])
    procs = [subprocess.Popen(cmd + ["--shard", "%d/%d" % (i, jobs)],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True)
             for i in range(jobs)]
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


#
# REFUSE TO START ON A TREE THAT IS ALREADY WRONG.
#
# This file no longer patches `src/` in place, so it can no longer be the thing
# that leaves a mutant behind.  This check stays anyway, because it is not
# really about this file: what it detects is a tree carrying a deliberate
# defect for ANY reason -- a run killed before the copy landed, a half-applied
# hand edit, a stale checkout, a merge that took the wrong side.
#
# EVERY test binary links ALL of `src/` (mutsnap.py's CLOSURE note), so ONE
# stray mutant anywhere makes EVERY suite's verdicts meaningless.  That is not
# hypothetical: it produced `cadence RUN FAILED` and `v34datapump FAILED` on
# two consecutive `--all` runs, while both suites passed standalone with
# identical counts, and the temptation was to blame concurrency and lower
# --jobs.  Lowering --jobs would have hidden it.
#
# It runs in the REAL tree and BEFORE anything is copied, so a tree in that
# state is never the thing that gets copied.  50 ms over 63 suites.  Finding
# 705 is the hazard; this is the guard that survives the copy.
#
LOCK = os.path.join("test", "mutations", ".running")


def live_mutants():
    """Sources that hold a mutation's `replace` and not its `find`."""
    out = []
    try:
        suites = json.load(open(SUITES))
    except (OSError, ValueError):
        return out
    for name, entry in sorted(suites.items()):
        if name.startswith("_") or not isinstance(entry, list) or len(entry) != 2:
            continue
        path = "test/mutations/%s.json" % name
        if not os.path.exists(path) or not os.path.exists(entry[0]):
            continue
        try:
            text = open(entry[0]).read()
            muts = json.load(open(path))
        except (OSError, ValueError):
            continue
        for m in muts:
            if not isinstance(m, dict) or "find" not in m or "replace" not in m:
                continue
            if m["find"] not in text and m["replace"] in text:
                out.append((entry[0], m.get("label", "?")))
    return out


def preflight():
    """Refuse to start against a tree that already holds a live mutant.

    Every invocation runs this, shards included, because it is 50 ms and it is
    the one hazard a copy cannot help with: copying a corrupted tree gives you
    a corrupted copy and verdicts that mean nothing.
    """
    bad = live_mutants()
    if not bad:
        return
    sys.stderr.write(
        "mutate.py: REFUSING TO RUN -- this tree already holds %d live "
        "mutant(s)\n" % len(bad))
    for path, label in bad:
        sys.stderr.write("    %s: %s\n" % (path, label))
    sys.stderr.write(
        "  Every test binary links all of src/, so any verdict measured now\n"
        "  is meaningless.  This runner works in a copy and cannot have left\n"
        "  these, so look for a hand edit, a bad merge, or a run from before\n"
        "  that change.  Restore with `git checkout -- <path>` and check\n"
        "  `git status` before retrying.\n")
    sys.exit(2)


def take_lock(root=None):
    """One run per COPY.

    This was one run per TREE, and it had to be: two runs writing one source
    file produce nonsense.  Now that each run mutates its own copy, two runs in
    one tree are CORRECT and refusing them would be the regression -- so the
    lock moved into the copy along with the mutation, which is the relaxation
    the hazard allows rather than a weakening of it.  What it still catches is
    a copy being used twice, and it still reports a lock whose owner has died.
    """
    lock = os.path.join(root or os.getcwd(), LOCK)
    if os.path.exists(lock):
        try:
            pid = int(open(lock).read().strip())
        except (OSError, ValueError):
            pid = -1
        alive = False
        if pid > 0:
            try:
                os.kill(pid, 0)
                alive = True
            except OSError:
                alive = False
        if alive:
            sys.exit("mutate.py: another run is already using %s (pid %d).  "
                     "Two runs mutating one copy produce nonsense; wait for it "
                     "or kill it with SIGTERM so its restore runs."
                     % (os.path.dirname(lock), pid))
        sys.stderr.write("mutate.py: stale lock from pid %d, taking it\n" % pid)
    with open(lock, "w") as f:
        f.write("%d\n" % os.getpid())
    #
    # Absolute, and captured now: this handler runs at interpreter exit, by
    # which time the cwd may be a directory that no longer exists.
    #
    atexit.register(lambda: os.path.exists(lock) and os.remove(lock))


#
# HALF THE CORES, NOT ALL OF THEM.
#
# Every mutation is a build plus a test run in its own copy, so N jobs is N
# concurrent compilers, and `--jobs $(nproc)` leaves nothing for the machine
# to be used with.  A full re-record is 4653 mutations and takes tens of
# minutes; making it unusable-for-anything-else for that whole time is a bad
# trade for the last few percent of throughput.
#
# It is a DEFAULT and not a cap: pass `--jobs N` for whatever N suits.  Note
# mutate.py's own note at the shard code that a real bug was found BECAUSE
# jobs were high and lowering them would have hidden it -- so this is a load
# choice, not a determinism one, and running higher deliberately is fine.
#
def default_jobs():
    try:
        return max(1, (os.cpu_count() or 2) // 2)
    except Exception:
        return 1



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
    ap.add_argument("--only", metavar="TEXT",
                    help="run only mutations whose label contains TEXT.  For "
                         "ITERATION: 1.4 s instead of a full suite.  The run "
                         "is marked SUBSET and cannot be recorded as a "
                         "baseline")
    ap.add_argument("--jobs", type=int, metavar="N", default=default_jobs(),
                    help="run the suite over N copies at once; each costs "
                         "about 21 MB of temporary disk and the verdicts are "
                         "identical to a serial run.  Default is HALF the "
                         "cores (%d here) -- see default_jobs()"
                         % default_jobs())
    ap.add_argument("--shard", metavar="I/N",
                    help=argparse.SUPPRESS)   # set by --jobs on its workers
    args = ap.parse_args()
    preflight()

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

    #
    # THE LEAF COPIES, AND ONLY THE LEAF.  `--all` and `--jobs` do not mutate
    # anything themselves; they spawn children that each do this for
    # themselves.  A parent that copied as well would have its children copy
    # the copy, and `run_all` would resolve `__file__` inside a temp directory
    # that is about to be deleted.
    #
    args.source = tree_relative(args.source, "source")
    args.test = tree_relative(args.test, "test binary")
    args.mutations = tree_relative(args.mutations, "mutation file")
    enter_workdir()
    take_lock()

    entries = json.load(open(args.mutations))
    muts = [m for m in entries if "find" in m]
    #
    # ITERATION IS THE COMMON CASE AND IT WAS PAYING FOR THE WHOLE SUITE.
    #
    # A batch changing one arm wants to know whether ITS mutations still fail,
    # and had no way to ask: `--suite` runs all of them.  Measured on the tree
    # today, one batch ran v34hstx1's 749 four times over fourteen minutes to
    # check work that touched a handful of labels.  One mutation is 1.4 s.
    #
    # THE SUBSET RUN IS MARKED, and that matters more than the speed.  Its
    # summary would otherwise be indistinguishable from a full run's -- the
    # exact shape of findings 347, 432, 540 and 542 -- so it does not print
    # the string `mutations:` at all, which is what `mutsnap.py` looks for.
    # A subset can never be recorded as a baseline by accident.
    #
    subset = None
    if args.only:
        subset = len(muts)
        muts = [m for m in muts if args.only in m.get("label", "")]
        if not muts:
            sys.exit("no mutation label contains %r" % args.only)
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

    # Everything below runs against a mutated COPY.  The restore is kept so
    # that the copy is self-consistent at every point a reader might look at
    # it; what is NOT kept is the rebuild that used to follow it, which existed
    # to leave the real tree's binaries matching its source and is 0.2 s of
    # pure waste in a directory about to be deleted.
    uncaught, broken, equivalent, surprises = [], [], [], []
    by = {"test": 0, "strings": 0, "hang": 0}
    try:
        rc, out, _ = build_and_run(target, args.test)
        if rc != 0:
            sys.exit("baseline is not green -- fix that first:\n" +
                     (out or "") + diverge_note(args.test, out or ""))

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
        if stdout:
            stdout.__exit__(None, None, None)

    if args.shard:
        print(SHARD_MARK + json.dumps({
            "text": held.getvalue(), "total": len(muts),
            "uncaught": uncaught, "broken": broken,
            "equivalent": equivalent, "surprises": surprises, "by": by}))
        return 0

    if subset is not None:
        print("\n  SUBSET: %d of %d mutations (--only %r) -- %d caught, "
              "%d NOT caught, %d unusable, %d equivalent.\n"
              "  This is an iteration aid.  It is NOT a suite result and "
              "cannot be a baseline."
              % (len(muts), subset, args.only,
                 len(muts) - len(uncaught) - len(broken) - len(equivalent),
                 len(uncaught), len(broken), len(equivalent)))
        return 1 if uncaught or surprises else 0

    return report(len(muts), uncaught, broken, equivalent, surprises, by)


if __name__ == "__main__":
    sys.exit(main())
