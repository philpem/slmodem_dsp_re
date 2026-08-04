#!/usr/bin/env python3
"""
Periodic mechanical mutation sweep, using mewt.

WHAT THIS IS FOR, AND WHAT IT IS NOT

`tools/mutate.py` runs mutations written BY HAND, each one a claim about the
reconstruction with the reading it tests in its label -- "swap CJ and JM in
the message timeout".  Those cannot be generated: mewt has no string-literal
operator and no operator that moves a statement, so most of what they say is
outside what a syntactic mutator can express.  They stay.

This is the other half.  mewt mutates mechanically and finds gaps nobody
thought to write a claim about, which it demonstrably does: over v8jm.c,
where all 29 hand-written entries were caught, it found fourteen more --
eight of them `initTxSequence`'s field clears, which delete cleanly because
the fixture hands in a buffer that is already zero, and the rest output bits
nothing asserts on.  Finding 193.

HIGH AND MEDIUM ONLY, by default.  Low severity came back 65.9% caught on
v8jm.c, which is 299 survivors to triage for one file, most of them noise of
the `x > 0` to `x != 0` kind.  High was 100% and medium 95.1%, and the 5% was
worth having.  `--severity low` if you want the rest.

TRIAGE.  Every survivor is one of three things and the point is to decide
which, not to drive the number to zero:

  a fixture gap     the commonest.  A statement that writes a zero cannot be
                    tested against a zeroed fixture.  Fix the fixture, not
                    the source.
  a real gap        nothing asserts on that output.  Add a hand-written entry
                    to the matching test/mutations/*.json, so the claim is
                    recorded rather than just the hole being plugged.
  equivalent        the mutant cannot change observable behaviour.  Record it
                    in the suite's json as `equivalent`, with why.

IT RUNS IN A THROWAWAY WORKTREE.  mewt edits the source in place to build
each mutant, and a crash mid-run would leave the tree mutated.  A detached
worktree beside the others costs a few seconds and makes that impossible.

INSTALLING IT.  mewt is AGPL-3.0, so it is a development dependency rather
than something this tree vendors -- the licence does not reach source it is
merely run over, but there is no reason to entangle the two.  Not on PATH:

    gh release download v4.0.0 -R trailofbits/mewt \\
       -p 'mewt-x86_64-unknown-linux-gnu.tar.xz*'
    sha256sum -c mewt-x86_64-unknown-linux-gnu.tar.xz.sha256
    tar xf mewt-x86_64-unknown-linux-gnu.tar.xz

then put it on PATH or point $MEWT at it.  The pinned checksum is below;
check it, because this runs a downloaded binary over the whole tree.
"""

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys

VERSION = "4.0.0"
SHA256 = "0e30678a7d090d112b9cf4d2d620053ca9da7c458ed5c2d94010304edded728b"
WORKTREE = "../mewt_sweep"          # beside the others: ../slmodemd must resolve


def find_mewt():
    m = os.environ.get("MEWT") or shutil.which("mewt")
    if not m:
        sys.exit(__doc__[__doc__.index("INSTALLING IT."):])
    v = subprocess.run([m, "--version"], capture_output=True, text=True).stdout
    if VERSION not in v:
        print("  note: pinned %s, found %s" % (VERSION, v.strip()))
    return m


def mapping():
    """source -> test binary, and the sources we have no pairing for.

    suites.json is authoritative because it was written after six sets were
    misread by guessing (finding 190).  The t_<basename> guess is a fallback
    and is reported as a guess, not silently trusted.
    """
    suites = {v[0]: v[1] for k, v in
              json.load(open("test/mutations/suites.json")).items() if k != "_"}
    tests = set(re.search(r"^TESTS\s*:=(.*)$",
                          open("Makefile").read(), re.M).group(1).split())
    known, guessed, unmapped = {}, {}, []
    for src in sorted(glob.glob("src/**/*.c", recursive=True)):
        base = os.path.basename(src)[:-2]
        if src in suites:
            known[src] = suites[src]
        elif "t_" + base in tests:
            guessed[src] = "build/test/t_" + base
        else:
            unmapped.append(src)
    return known, guessed, unmapped


def config(targets, severities):
    per = "".join(
        '\n[[per_target]]\nglob = "%s"\ntest.cmd = '
        '"make -s %s >/dev/null 2>&1 && ./%s >/dev/null 2>&1"\n'
        'test.timeout = 120\n' % (src, tst, tst)
        for src, tst in sorted(targets.items()))
    return ('db = "mewt.sqlite"\n[log]\nlevel = "info"\n'
            '[targets]\ninclude = [%s]\n'
            # A target with no per_target rule would fall back to this, which
            # is correct but fifty times slower; nothing should reach it.
            '[test]\ncmd = "make -s test >/dev/null 2>&1"\ntimeout = 600\n'
            % ", ".join('"%s"' % s for s in sorted(targets))) + per


def main():
    ap = argparse.ArgumentParser(description="Mechanical mutation sweep (mewt).")
    ap.add_argument("--severity", default="high,medium")
    ap.add_argument("--guessed", action="store_true",
                    help="also sweep files paired by the t_<basename> guess")
    ap.add_argument("--only", metavar="GLOB", help="restrict to one source")
    ap.add_argument("--keep", action="store_true",
                    help="leave the worktree and its database in place")
    args = ap.parse_args()

    mewt = find_mewt()
    known, guessed, unmapped = mapping()
    targets = dict(known)
    if args.guessed:
        targets.update(guessed)
    if args.only:
        targets = {s: t for s, t in targets.items() if glob.fnmatch.fnmatch(s, args.only)}
    if not targets:
        sys.exit("nothing to sweep")

    print("  sweeping %d source file(s) at severity %s" % (len(targets), args.severity))
    if not args.guessed and guessed:
        print("  %d more could be swept with --guessed (pairing inferred, "
              "not from suites.json)" % len(guessed))
    if unmapped:
        # Said out loud every run: these are the files no mutation of any kind
        # reaches, which is a coverage statement about the suite itself.
        print("  %d source file(s) have NO test binary and are swept by "
              "nothing:\n     %s" % (len(unmapped), " ".join(unmapped)))

    root = os.path.abspath(".")
    wt = os.path.abspath(WORKTREE)
    subprocess.run(["git", "worktree", "remove", "--force", wt],
                   capture_output=True)
    r = subprocess.run(["git", "worktree", "add", "-f", "--detach", wt, "HEAD"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("could not make the worktree:\n" + r.stderr)
    try:
        open(os.path.join(wt, "mewt.toml"), "w").write(
            config(targets, args.severity))
        env = dict(os.environ)
        for cmd in (["mutate"], ["run"]):
            subprocess.run([mewt] + cmd, cwd=wt, env=env)
        out = subprocess.run([mewt, "results", "--severity", args.severity],
                             cwd=wt, env=env, capture_output=True, text=True)
        print(out.stdout)
        # Keep the report where it can be diffed against the last sweep.
        os.makedirs(os.path.join(root, "docs"), exist_ok=True)
        with open(os.path.join(root, "docs", "mewt_survivors.txt"), "w") as f:
            f.write("mewt %s, severity %s -- survivors to triage.\n"
                    "See tools/mewtsweep.py for what the three outcomes are.\n\n"
                    % (VERSION, args.severity))
            f.write(out.stdout)
        print("  written to docs/mewt_survivors.txt")
    finally:
        if not args.keep:
            subprocess.run(["git", "worktree", "remove", "--force", wt],
                           capture_output=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
