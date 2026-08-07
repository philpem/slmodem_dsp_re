#!/usr/bin/env python3
"""
What every mutation suite last said, and whether that is still true of THIS tree.

WHY THIS EXISTS

Every batch here runs its suites twice -- once for a baseline, once after --
because `make phase` cannot see a lost mutation: an UNUSABLE mutation does not
fail a run (finding 347) and four batches have silently lost mutations that
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
test binary links ALL of `src/`.  So editing `v34rx.c` can change
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
`v34hshak.c` still has to run all six suites pinned to it before it can claim
anything.  What it removes is the BEFORE pass, for suites whose key still
matched at the fork point.  "Baselines are committed now" does not mean "skip
the before" -- it means the before was already run, by whoever last touched
the tree, and is on record with the key to prove it.

DETERMINISM

Verdicts have to be reproducible or none of this holds.  `v34hshak`'s 209
were identical across a serial run and two eight-way parallel runs (finding
541), which is the evidence for it.  The one classification that is NOT
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
CLOSURE = ("Makefile", "src", "include", os.path.join("test", "harness"))

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


def suite_key(name, entry):
    """The one string that says which tree these verdicts describe."""
    h = hashlib.sha256()
    for p in CLOSURE:
        if os.path.exists(p):
            digest_path(h, p)
    # the suite's own driver, e.g. build/test/t_v34hshak -> test/unit/t_v34hshak.c
    driver = os.path.join("test", "unit", os.path.basename(entry[1]) + ".c")
    if os.path.exists(driver):
        digest_path(h, driver)
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


VERDICT = [
    (re.compile(r"^  ok    (.{1,52}?)\s*  caught \((\w+)\)$"), "caught"),
    (re.compile(r"^  \*\*\*\*  (.{1,52}?)\s*  NOT CAUGHT$"), "uncaught"),
    (re.compile(r"^  ==    (.{1,52}?)\s*  survived, equivalent$"), "equivalent"),
    (re.compile(r"^  ----  (.{1,52}?)\s*  did not compile$"), "unusable"),
    (re.compile(r"^  \?\?\?\?  (.{1,52}?)\s*  ANCHOR MATCHES \d+ TIMES$"),
     "unusable"),
    (re.compile(r"^  !!    (.{1,52}?)\s*  CAUGHT, recorded as equivalent$"),
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
    for name in names:
        if name not in reg:
            print("  no such suite: %s" % name)
            return 1
        verdicts, summary, out = run_suite(name, args.jobs)
        if not verdicts and "FAILED" in summary:
            print("  %-16s RUN FAILED -- not recorded\n%s" % (name, out[-800:]))
            return 2
        snap["suites"][name] = {
            "key": suite_key(name, reg[name]),
            "summary": summary,
            "verdicts": verdicts,
        }
        print("  %-16s %s" % (name, summary))
    json.dump(snap, open(SNAP, "w"), indent=1, sort_keys=True)
    print("\n  wrote %s (%d suite(s) recorded)" % (SNAP, len(snap["suites"])))
    return 0


def cmd_check(args):
    snap = load(SNAP, {})
    reg = registered()
    have = snap.get("suites", {})
    current, stale, missing = [], [], []
    for name in sorted(reg):
        e = have.get(name)
        if not e:
            missing.append(name)
        elif e.get("key") == suite_key(name, reg[name]):
            current.append(name)
        else:
            stale.append(name)
    for n in stale:
        print("  STALE    %-16s %s" % (n, have[n].get("summary", "")))
    for n in missing:
        print("  MISSING  %-16s never recorded" % n)
    print("\n  %d current, %d stale, %d never recorded, of %d registered"
          % (len(current), len(stale), len(missing), len(reg)))
    if stale or missing:
        print("  A stale entry is not a baseline.  Re-run those suites and")
        print("  `tools/mutsnap.py --update <suite>...` before quoting them.")
    return 1 if (stale or missing) else 0


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
    ap.add_argument("--jobs", type=int, metavar="N",
                    help="passed through to mutate.py")
    ap.add_argument("suite", nargs="*", help="default: all of them")
    args = ap.parse_args()
    if args.update:
        return cmd_update(args)
    if args.verify:
        return cmd_verify(args)
    return cmd_check(args)


if __name__ == "__main__":
    sys.exit(main())
