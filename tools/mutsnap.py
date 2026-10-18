#!/usr/bin/env python3
"""Optional historical mutation records and source-dependency hashes.

No build, merge or release gate requires these records to be current.
--check inspects bookkeeping without executing mutants. --update and --verify
require explicit suite names; there is no implicit whole-tree campaign.

The coarse key covers Makefile, src/, include/, test/harness/, tools/mutate.py,
the suite driver and its mutation JSON. Tests link all of src/, so a key over
only the mutated translation unit would miss dependencies. The key does not
identify the compiler or all external inputs: a matching key alone cannot
establish a comparable baseline. Record compiler identity with any new result.
See docs/method/mutation-policy.md for the policy and evidence limits.
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
# caught, unusable or equivalent is its code. Its shared mutation-source
# resolver also decides which physical file is changed. Both belong here;
# changing either can invalidate historical verdicts.
#
CLOSURE = ("Makefile", "src", "include", os.path.join("test", "harness"),
           os.path.join("tools", "mutate.py"),
           os.path.join("tools", "mutation_source.py"))

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
# `--check` is optional inspection. Hash the shared closure once so its cost
# does not grow with the number of suites.  Hashing Makefile + src/ +
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
        with open(path) as stream:
            return json.load(stream)
    except (OSError, ValueError):
        return default


def registered():
    return {k: v for k, v in load(SUITES, {}).items()
            if not k.startswith("_") and isinstance(v, list) and len(v) == 2}


DIVERGE = os.path.join("tools", "gccdiverge.json")


def _declared_cache():
    """basename -> tools/gccdiverge.json entry, loaded once per process."""
    if not _DECLARED_CACHE:
        _DECLARED_CACHE.append({
            k: v for k, v in load(DIVERGE, {}).items() if not k.startswith("_")})
    return _DECLARED_CACHE[0]


_DECLARED_CACHE = []


def declared_entry(entry):
    """The gccdiverge entry for a suite's test binary, or None.

    A binary with an entry in tools/gccdiverge.json exits non-zero on the
    UNMUTATED modern source, and tools/mutate.py judges a mutant caught by a
    non-zero exit -- so it cannot distinguish "the mutation was caught" from
    "this baseline was already red" and refuses the SUITE, not the row
    (findings F2157 and F3002).  Such a suite can therefore NEVER be recorded,
    and it must not read as either of the two things the old three-class check
    would have called it:

      * not STALE, which says "re-run this and it will go current"; and
      * not MISSING, which `cmd_check` treats as a hard defect for a registered
        suite that was never recorded.

    It is its own class, printed with the register key and finding so the
    reason travels with it, and counted on the verdict line so the denominator
    cannot hide it.  Finding F6002 names this class as the prerequisite for
    retiring `psd`'s and `v90equ`'s entries without rotting their anchors out
    of anchorcheck.py's sweep.  `mutsnap.py` is not in the mutation closure
    (Makefile, src/, include/, test/harness/, tools/mutate.py), so this change
    invalidates no recorded key.
    """
    if not entry or len(entry) != 2:
        return None
    return _declared_cache().get(os.path.basename(entry[1]))


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
    # Subsets can carry real per-mutation verdict lines, but they are never
    # a complete suite measurement. Discard them before update can record them.
    if re.search(r"^\s*SUBSET:", r.stdout, re.MULTILINE):
        return {}, "FAILED (SUBSET cannot be recorded)", r.stdout
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
    if not summary:
        # A failed shard may already have printed valid partial verdicts.
        # Without the complete summary none of those are recordable either.
        return {}, "FAILED", r.stdout
    return verdicts, summary[-1].strip(), r.stdout


def cmd_update(args):
    snap = load(SNAP, {})
    snap.setdefault("_", HEADER)
    snap.setdefault("suites", {})
    reg = registered()
    names = args.suite
    failed = []
    for name in names:
        if name not in reg:
            print("  no such suite: %s" % name)
            return 1
        #
        # A DECLARED BINARY IS SKIPPED BEFORE THE RUN, not after it.  mutate.py
        # would refuse its baseline anyway; running it first only spends the
        # copy-and-build and then reports a failure that is expected.  Nothing
        # is written, so any existing entry -- itself unquotable, being stale --
        # is left alone rather than refreshed or deleted.
        #
        d = declared_entry(reg[name])
        if d is not None:
            print("  %-16s DECLARED -- %s in tools/gccdiverge.json (finding "
                  "%s); cannot be scored, left as-is"
                  % (name, os.path.basename(reg[name][1]), d.get("finding", "?")))
            continue
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
    with open(SNAP, "w") as stream:
        json.dump(snap, stream, indent=1, sort_keys=True)
    print("\n  wrote %s (%d suite(s) recorded)" % (SNAP, len(snap["suites"])))
    if failed:
        print("  %d suite(s) COULD NOT RUN and are still stale: %s"
              % (len(failed), ", ".join(failed)))
        print("  A suite with no baseline is not a suite that passed.")
        return 2
    return 0


#
# Optional inspection fails on missing, orphaned or inconsistent records.
# Staleness is reported; --strict can fail on it for an explicit investigation.
# Neither mode is a merge requirement, and neither executes mutants.
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
    current, stale, missing, bad, declared = [], [], [], [], []
    for name in sorted(reg):
        e = have.get(name)
        #
        # DECLARED FIRST, so a suite whose binary is in tools/gccdiverge.json
        # is never counted as stale or missing whatever its recorded key says.
        # The class is about the BINARY and not the record: a declared binary
        # cannot be scored no matter how fresh the entry looks.
        #
        d = declared_entry(reg[name])
        if d is not None:
            declared.append((name, os.path.basename(reg[name][1]),
                             d.get("finding", "?")))
        elif not e:
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
    for n, binary, finding in declared:
        print("  DECLARED %-16s %s is in tools/gccdiverge.json (finding %s) -- "
              "unscoreable on this build, neither stale nor missing"
              % (n, binary, finding))
    for n in missing:
        print("  MISSING  %-16s registered, never recorded" % n)
    for n in orphaned:
        print("  ORPHANED %-16s recorded, no longer in suites.json" % n)
    for n, why in bad:
        print("  INCONSISTENT %-16s %s" % (n, why))
    print("\n  mutation snapshot: %d current, %d stale, %d never recorded, "
          "%d declared-unscoreable, of %d registered"
          % (len(current), len(stale), len(missing), len(declared), len(reg)))
    if stale:
        print("  Stale entries are historical evidence, not current baselines. "
              "No refresh is required; run named suites only when needed.")
    if declared:
        print("  A declared suite is not a defect either: its binary exits "
              "non-zero on the unmutated modern source, so mutate.py cannot "
              "score it (F2157/F3002).  Its anchors stay in anchorcheck's "
              "sweep and its record is left as it was.")
    hard = missing or orphaned or bad
    return 1 if (hard or (args.strict and stale)) else 0


def cmd_verify(args):
    """Run the suites now and diff every LABEL against the record."""
    snap = load(SNAP, {})
    reg = registered()
    have = snap.get("suites", {})
    names = args.suite
    bad = 0
    for name in names:
        if name in reg and declared_entry(reg[name]) is not None:
            print("  %-16s DECLARED -- cannot be scored" % name)
            continue
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
                    help="inspect source keys and bookkeeping without executing mutants")
    ap.add_argument("--verify", action="store_true",
                    help="run the suites and diff every label against the "
                         "record -- the mechanical form of 'the same seven "
                         "NOT CAUGHT, matched by name'")
    ap.add_argument("--strict", action="store_true",
                    help="also fail on stale source keys during optional inspection; "
                         "never a merge requirement")
    ap.add_argument("--jobs", type=int, metavar="N",
                    help="worker count passed through to mutate.py for named suites")
    ap.add_argument("suite", nargs="*", help="explicit suites for --update or --verify")
    args = ap.parse_args()
    if sum((args.update, args.check, args.verify)) > 1:
        ap.error("choose only one of --update, --check or --verify")
    if args.check and args.suite:
        ap.error("--check inspects all records; suite names select execution only")
    if (args.update or args.verify) and not args.suite:
        ap.error("--update and --verify require explicit suite names; "
                 "whole-tree refreshes are retired")
    if args.suite and not (args.update or args.verify):
        ap.error("suite names require --update or --verify")
    if args.update:
        return cmd_update(args)
    if args.verify:
        return cmd_verify(args)
    return cmd_check(args)


if __name__ == "__main__":
    sys.exit(main())
