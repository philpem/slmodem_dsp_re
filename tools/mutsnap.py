#!/usr/bin/env python3
"""
What every mutation suite last said, and whether that is still true of THIS tree.

WHY THIS EXISTS

Every batch here runs its suites twice -- once for a baseline, once after --
because `make phase` cannot see a lost mutation: an UNUSABLE mutation does not
fail a run (finding F347) and four batches have silently lost mutations that
way.  The baseline half is pure repetition of what the previous batch already
measured, and it is half the mutation cost of every batch.

The obvious fix is to write the numbers down.  The tree has been doing that
for a year, in `docs/findings.md`, and it does not work: those numbers are
snapshots from whichever batch created each suite and nothing has ever made
them keep up.  `v34hsmst44` is recorded at 77 mutations and has 215.  A number
nobody can tell is stale is worse than no number, because it gets quoted.

So this file records the verdicts WITH A KEY THAT SAYS WHAT THEY DESCRIBE.  If
the key still matches, the recorded verdicts are valid by construction and a
batch may use them as its baseline.  If it does not, the entry says so and the
suite has to be run.  Staleness becomes detectable WITHOUT running anything,
which is the whole point -- checking it by re-measuring is the cost we are
trying to avoid.

WHAT THE KEY COVERS, AND WHY IT IS COARSE ON PURPOSE

A first design keyed each suite on its own mutated source plus its mutation
JSON.  That is UNSOUND.  A suite's verdicts depend on everything linked into
its test binary, and `OBJ` in the Makefile is `find src -name '*.c'` -- every
test binary links ALL of `src/`.  So editing `V34RX.c` can change
`v34hshak`'s verdicts while both of those hashes still match, and the snapshot
would read valid while being wrong.  That is the failure mode this file exists
to kill, reintroduced one level down.

The key is therefore over everything that can reach the binary:

    Makefile, src/, include/, test/harness/    -- the shared closure
    test/unit/t_<binary>.c                     -- the suite's own driver
    test/mutations/<suite>.json                -- its mutations

Coarse, so almost any edit invalidates almost every entry.  That is correct
rather than unfortunate: with every binary linking every object, an edit in
`src/` genuinely can move any suite, and a precise key here would be a
precise lie.  Coarse-and-correct beats precise-and-unsound for a gate whose
only job is detecting staleness.

WHAT IT DOES NOT DO

It does not make the AFTER pass free, and nothing can: a batch that changes
`V34hshak.c` still has to run all six suites pinned to it before it can claim
anything.  What it removes is the BEFORE pass, for suites whose key still
matched at the fork point.  "Baselines are committed now" does not mean "skip
the before" -- it means the before was already run, by whoever last touched
the tree, and is on record with the key to prove it.

DETERMINISM

Verdicts have to be reproducible or none of this holds.  `v34hshak`'s 209
were identical across a serial run and two eight-way parallel runs (finding
F541), which is the evidence for it.  The one classification that is NOT
reproducible in principle is `hang` -- a mutation caught by the 120-second
timeout rather than by a check -- because a loaded machine can move it.  No
suite in this tree has ever produced one; if one appears, it is recorded as
`hang` and exempted from the comparison, and this comment stops being true.
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

SNAP = os.path.join("test", "mutations", "snapshot.json")
SUITES = os.path.join("test", "mutations", "suites.json")
#
# `tools/mutate.py` is IN the closure: it is the runner, and what counts as
# caught, unusable or equivalent is its code.  Change it and these verdicts
# describe a classification that no longer exists.  Nothing else under
# `tools/` can move a verdict, so nothing else is here.
#
CLOSURE = ("Makefile", "src", "include", os.path.join("test", "harness"),
           os.path.join("tools", "mutate.py"))

HEADER = [
    "What every mutation suite last said, and the key saying what it",
    "described.  Written by tools/mutsnap.py --update; see the head of that",
    "file for why a key is the point and a bare number is not.",
    "An entry whose key no longer matches the tree is STALE: its verdicts",
    "were true of a different tree and must not be quoted as a baseline.",
]


def digest_path(h, path):
    """Hash a file, or every file under a directory, in a stable order."""
    if os.path.isfile(path):
        h.update(path.encode())
        h.update(open(path, "rb").read())
        return
    for root, dirs, files in os.walk(path):
        dirs.sort()
        for f in sorted(files):
            p = os.path.join(root, f)
            h.update(p.encode())
            h.update(open(p, "rb").read())


#
# THE SHARED CLOSURE IS HASHED ONCE, NOT ONCE PER SUITE.
#
# `--check` runs inside `make test` inside `make phase`, so it is on the path
# everybody takes constantly and has to be cheap.  Hashing Makefile + src/ +
# include/ + test/harness/ is ~8 MB; doing it for each of 48 suites is 384 MB
# of pointless work for a value that is identical every time.
#
_CLOSURE_CACHE = []


def closure_digest():
    if not _CLOSURE_CACHE:
        h = hashlib.sha256()
        for p in CLOSURE:
            if os.path.exists(p):
                digest_path(h, p)
        _CLOSURE_CACHE.append(h.hexdigest())
    return _CLOSURE_CACHE[0]


def suite_key(name, entry):
    """The one string that says which tree these verdicts describe."""
    h = hashlib.sha256()
    h.update(closure_digest().encode())
    #
    # THE DRIVER MAY BE `.c` OR `.cpp`, AND ASSUMING `.c` TURNED THE DETECTOR
    # OFF FOR MOST OF THE TREE.  This built the path as basename + ".c"
    # literally, so for the 72 of 123 suites driven by a C++ test the
    # `os.path.exists` below simply failed and the driver was hashed into the
    # key not at all.  Their recorded verdicts then stayed CURRENT however the
    # test changed -- a C++ test could be rewritten or gutted and every
    # `caught` would still read as valid by construction, which is the one
    # thing this file exists to prevent.  Finding F1451; it is finding F1383's
    # third instance of a check that was silently not checking.
    #
    # Both extensions are tried and the first that exists is hashed.  A suite
    # whose driver is missing entirely still keys on the closure and its own
    # mutation set, as before.
    #
    base = os.path.join("test", "unit", os.path.basename(entry[1]))
    for driver in (base + ".c", base + ".cpp"):
        if os.path.exists(driver):
            digest_path(h, driver)
            break
    muts = os.path.join("test", "mutations", name + ".json")
    if os.path.exists(muts):
        digest_path(h, muts)
    return h.hexdigest()


def load(path, default):
    try:
        return json.load(open(path))
    except (OSError, ValueError):
        return default


def registered():
    return {k: v for k, v in load(SUITES, {}).items()
            if not k.startswith("_") and isinstance(v, list) and len(v) == 2}


#
# `%-52s` PADS BUT DOES NOT TRUNCATE.
#
# A first version capped the label at 52 characters, which is the field width
# mutate.py formats with -- so every label LONGER than 52 printed in full and
# matched nothing.  21 of v34hshak's 209 are that long, and the snapshot
# recorded 188 verdicts while its own summary said 209.  The INCONSISTENT
# check below caught it on the first run, which is the entire argument for
# having a tool check itself against its own second measurement.
#
# The separator is what to anchor on: mutate.py writes two literal spaces
# after the padded field, so there are always AT LEAST two.
#
VERDICT = [
    (re.compile(r"^  ok    (.+?)\s{2,}caught \((\w+)\)$"), "caught"),
    (re.compile(r"^  \*\*\*\*  (.+?)\s{2,}NOT CAUGHT$"), "uncaught"),
    (re.compile(r"^  ==    (.+?)\s{2,}survived, equivalent$"), "equivalent"),
    (re.compile(r"^  ----  (.+?)\s{2,}did not compile$"), "unusable"),
    (re.compile(r"^  \?\?\?\?  (.+?)\s{2,}ANCHOR MATCHES \d+ TIMES$"),
     "unusable"),
    (re.compile(r"^  \?\?\?\?  (.+?)\s{2,}VACUOUS -- REPLACE == FIND$"),
     "unusable"),
    (re.compile(r"^  !!    (.+?)\s{2,}CAUGHT, recorded as equivalent$"),
     "surprise"),
]


def run_suite(name, jobs):
    """Run one suite and return {label -> verdict} plus the summary line.

    Labels are truncated to 52 characters by mutate.py's own output, which is
    what makes them comparable between runs: both sides are truncated the same
    way.  A pair of labels agreeing for 52 characters and differing after is
    a collision this cannot see, and `refcheck.py` does not check for it --
    recorded here rather than guarded, because it has not happened.
    """
    cmd = [sys.executable, "tools/mutate.py", "--suite", name]
    if jobs:
        cmd += ["--jobs", str(jobs)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    verdicts = {}
    for line in r.stdout.split("\n"):
        for rx, v in VERDICT:
            m = rx.match(line)
            if m:
                #
                # `surprise` is recorded-equivalent-and-caught-anyway, and
                # mutate.py prints BOTH the `!!` line and an `ok` line for it.
                # The `!!` must win or the snapshot would call it plain
                # caught and lose the one thing that entry is flagging.
                #
                if v == "surprise" or m.group(1) not in verdicts:
                    verdicts[m.group(1)] = v
                break
    summary = [l for l in r.stdout.split("\n") if "mutations:" in l]
    return verdicts, (summary[-1].strip() if summary else "FAILED"), r.stdout


def cmd_update(args):
    snap = load(SNAP, {})
    snap.setdefault("_", HEADER)
    snap.setdefault("suites", {})
    reg = registered()
    names = args.suite or sorted(reg)
    failed = []
    for name in names:
        if name not in reg:
            print("  no such suite: %s" % name)
            return 1
        verdicts, summary, out = run_suite(name, args.jobs)
        if not verdicts and "FAILED" in summary:
            #
            # DO NOT ABORT, AND DO NOT RECORD.  This used to `return 2` on the
            # first suite that would not run, which threw away every suite
            # already measured and never reached the rest -- one unrunnable
            # suite cost the whole tier.  `psd` is the live example: `t_psd`
            # is exempted from the modern build (finding F1453) so its baseline
            # is not green, mutate.py rightly refuses to judge mutations
            # against a test that already fails, and 122 healthy suites went
            # unrecorded with it.
            #
            # Nothing is written for it, which is the honest outcome: no
            # baseline exists, so the entry stays STALE and cannot be quoted.
            # The run continues, the exit code is non-zero, and the names are
            # printed again at the end so a long log cannot bury them.
            #
            print("  %-16s COULD NOT RUN -- left stale, not recorded" % name)
            print("%s" % out[-400:])
            failed.append(name)
            continue
        snap["suites"][name] = {
            "key": suite_key(name, reg[name]),
            "summary": summary,
            "verdicts": verdicts,
        }
        print("  %-16s %s" % (name, summary))
    json.dump(snap, open(SNAP, "w"), indent=1, sort_keys=True)
    print("\n  wrote %s (%d suite(s) recorded)" % (SNAP, len(snap["suites"])))
    if failed:
        print("  %d suite(s) COULD NOT RUN and are still stale: %s"
              % (len(failed), ", ".join(failed)))
        print("  A suite with no baseline is not a suite that passed.")
        return 2
    return 0


#
# WHAT FAILS, AND WHY STALE DOES NOT
#
# The obvious wiring is "fail if any entry is stale", and it is wrong.  This
# runs inside `make test`, which runs inside `make phase`, which CLAUDE.md
# tells everyone to run constantly.  Almost any edit to `src/` invalidates
# almost every entry, so a strict gate here is RED from a batch's first edit
# until it has re-run its suites -- which is most of a batch's life.
#
# Two things then happen, and the second is the dangerous one.  People learn
# to ignore a red that is red by default.  And the cheapest way to clear it is
# `mutsnap --update`, which would refresh the record with numbers nobody
# examined -- manufacturing exactly the false baseline this file exists to
# prevent, now with a key attached vouching for it.  A snapshot refreshed as a
# chore is worse than no snapshot: `docs/findings.md`'s stale numbers at least
# LOOK old.
#
# It is the same lesson as the `tag:` heuristic that was measured and deleted
# (six flags, six false): a check that cries wolf is worse than no check.
#
# So staleness is REPORTED and does not fail -- it is the normal state during
# work, and `--verify` and the STALE label already stop a stale entry being
# quoted as a baseline.  What fails is what is always a defect:
#
#   MISSING       a registered suite that has never been recorded at all
#   ORPHANED      a recorded suite no longer in suites.json
#   INCONSISTENT  an entry whose summary line disagrees with its own
#                 per-label verdicts -- i.e. one edited by hand
#
# `--strict` adds staleness, and is what a MERGE has to pass: at a merge the
# work has just been verified, so refreshing the record is honest there and
# nowhere else.
#
def summary_counts(text):
    """(mutations, uncaught, unusable, equivalent) out of the summary line."""
    n = re.findall(r"(\d+) mutations|(\d+) NOT caught|(\d+) unusable|"
                   r"(\d+) equivalent", text)
    got = [int(x) for grp in n for x in grp if x]
    return got[:4] if len(got) >= 4 else None


def cmd_check(args):
    snap = load(SNAP, {})
    reg = registered()
    have = snap.get("suites", {})
    current, stale, missing, bad = [], [], [], []
    for name in sorted(reg):
        e = have.get(name)
        if not e:
            missing.append(name)
        elif e.get("key") == suite_key(name, reg[name]):
            current.append(name)
        else:
            stale.append(name)
    orphaned = sorted(set(have) - set(reg))
    #
    # The hand-editing check.  A key vouches for verdicts that were MEASURED;
    # nothing stops someone typing a number into the summary, and that number
    # is what gets quoted.  So the summary has to agree with the verdicts the
    # same run recorded.
    #
    for name, e in sorted(have.items()):
        got = summary_counts(e.get("summary", ""))
        v = e.get("verdicts", {})
        if not got or not v:
            continue
        total, uncaught, unusable, equiv = got
        actual = (len(v),
                  sum(1 for x in v.values() if x == "uncaught"),
                  sum(1 for x in v.values() if x == "unusable"),
                  sum(1 for x in v.values() if x == "equivalent"))
        #
        # An unusable mutation DOES reach a verdict line -- `mutate.py` prints
        # `????` for it and the parser records it as `unusable` -- so the map
        # is complete and `len(v)` is the total.  A first version added
        # `unusable` on top, on the assumption that those entries were
        # missing, and flagged the one suite in the tree that HAS an unusable
        # mutation (`v90equ`, 1) while agreeing with itself everywhere else.
        # Wrong by exactly the number of cases that could distinguish it,
        # which is the shape of every bug this file exists to catch.
        #
        if (actual[0], actual[1], actual[3]) != (total, uncaught, equiv):
            bad.append((name, "summary says %s, verdicts say %s"
                        % (got, actual)))
    for n in stale:
        print("  stale    %-16s %s" % (n, have[n].get("summary", "")))
    for n in missing:
        print("  MISSING  %-16s registered, never recorded" % n)
    for n in orphaned:
        print("  ORPHANED %-16s recorded, no longer in suites.json" % n)
    for n, why in bad:
        print("  INCONSISTENT %-16s %s" % (n, why))
    print("\n  mutation snapshot: %d current, %d stale, %d never recorded, "
          "of %d registered" % (len(current), len(stale), len(missing),
                                len(reg)))
    if stale:
        print("  A stale entry is not a baseline -- re-run those suites "
              "rather than quoting it.")
    hard = missing or orphaned or bad
    return 1 if (hard or (args.strict and stale)) else 0


def cmd_verify(args):
    """Run the suites now and diff every LABEL against the record."""
    snap = load(SNAP, {})
    reg = registered()
    have = snap.get("suites", {})
    names = args.suite or sorted(have)
    bad = 0
    for name in names:
        e = have.get(name)
        if not e:
            print("  %-16s never recorded" % name)
            bad += 1
            continue
        verdicts, summary, _ = run_suite(name, args.jobs)
        was, now = e["verdicts"], verdicts
        moved = sorted(set(was) | set(now),
                       key=lambda l: (was.get(l, ""), l))
        moved = [l for l in moved if was.get(l) != now.get(l)]
        if moved:
            bad += 1
            print("  %-16s %d verdict(s) MOVED" % (name, len(moved)))
            for l in moved:
                print("      %-52s %s -> %s"
                      % (l, was.get(l, "(absent)"), now.get(l, "(absent)")))
        else:
            print("  %-16s %d verdict(s), all unchanged" % (name, len(now)))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--update", action="store_true",
                    help="run the suites and record what they said")
    ap.add_argument("--check", action="store_true",
                    help="hash only: which recorded entries still describe "
                         "this tree.  Runs nothing and takes milliseconds")
    ap.add_argument("--verify", action="store_true",
                    help="run the suites and diff every label against the "
                         "record -- the mechanical form of 'the same seven "
                         "NOT CAUGHT, matched by name'")
    ap.add_argument("--strict", action="store_true",
                    help="also fail on a STALE entry.  What a MERGE has to "
                         "pass; deliberately not what `make phase` runs, see "
                         "the comment above cmd_check")
    ap.add_argument("--jobs", type=int, metavar="N",
                    help="passed through to mutate.py.  Unset means mutate.py "
                         "picks, which is HALF the cores -- deliberately not "
                         "all of them, since a full re-record is tens of "
                         "minutes and should leave the machine usable")
    ap.add_argument("suite", nargs="*", help="default: all of them")
    args = ap.parse_args()
    if args.update:
        return cmd_update(args)
    if args.verify:
        return cmd_verify(args)
    return cmd_check(args)


if __name__ == "__main__":
    sys.exit(main())
