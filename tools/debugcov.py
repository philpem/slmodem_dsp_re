#!/usr/bin/env python3
"""
Which diagnostic call sites does the test suite actually EXECUTE?

WHY THIS EXISTS

Three checks already look at the call sites, and none of them answers this.

  debugaudit.py --missing    counts sites in the blob against sites in our
                             source.  Says a site EXISTS, not that anything
                             runs it.
  debugaudit.py --invented   every literal in src/ against the blob's .rodata
                             and .data.  Says the STRING is the original's.  A
                             correct string at a site no test reaches passes
                             this without a murmur.
  mutate.py                  breaks a site and checks something goes red.  The
                             strongest of the three, but a mutation caught by
                             the string sweep rather than by a differential
                             test is telling you the literal exists, which is
                             the first check again.

`gcov` answers the remaining question directly: after the whole suite has run,
which `dsplibs_debug_printf` lines have a hit count of zero?

It agrees with the hand analysis where the two overlap, which is the reason to
trust it: finding 154 worked out by hand that 3 of `CALLPROG_Progress`'s sites
were verified and the rest were not, and this reports 3 live sites in
callprog.c and 30 dead, having been told nothing.

WHY THE WHOLE SUITE IN ONE RUN

Every test binary links the same objects, and the .gcda path is baked into
each object at compile time, so the runs ACCUMULATE.  Running all 62 binaries
against one instrumented tree gives suite-wide coverage without merging
anything by hand.

The tests set `dsplibs_debug_level` per case and the transcript blocks already
sweep 1 to 3, so a plain run reaches every site any test drives at any level.
There is nothing to configure.

WHAT INSTRUMENTATION COSTS

Nothing measurable, which was not a given.  `--coverage` disables some
optimisation, and with `-mfpmath=387` that can move x87 spill points and
change the rounding of intermediate values -- the exact class of difference
the differential tier exists to detect.  It does not happen here: all 62
binaries pass instrumented, the float-heavy ones (t_v34ec, t_v34rx) included.
See finding 192.  Worth re-checking if the flags ever change.

    tools/debugcov.py                 build, run, report
    tools/debugcov.py --no-build      reuse an existing build-cov tree
    tools/debugcov.py --summary       the two counts only, for `make phase`
    tools/debugcov.py --lines         whole-file line coverage as well
"""

import glob
import os
import re
import subprocess
import sys

BUILD = "build-cov"
COV = ("-Wall -Wextra -Wno-unused-parameter -g -O2 -Iinclude -MMD -MP "
       "--coverage")
LD = "-no-pie -Wl,-z,noexecstack,-z,notext --coverage"


def tests():
    m = re.search(r"^TESTS\s*:=(.*)$", open("Makefile").read(), re.M)
    return m.group(1).split()


def build():
    targets = ["%s/test/%s" % (BUILD, t) for t in tests()]
    r = subprocess.run(["make", "-j8", "BUILD=" + BUILD, "CFLAGS=" + COV,
                        "LDFLAGS=" + LD] + targets,
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("instrumented build failed:\n" + r.stdout[-4000:] + r.stderr[-4000:])
    return targets


def run(targets):
    for f in glob.glob("%s/**/*.gcda" % BUILD, recursive=True):
        os.unlink(f)                        # counts must not carry over
    bad = [t for t in targets
           if subprocess.run([t], capture_output=True).returncode != 0]
    if bad:
        #
        # THIS one is fatal, and it is the only thing here that is -- for the
        # ordinary reason, not a special one: the goal is a replacement that
        # behaves identically to the blob, so ANY test disagreeing with the
        # blob is a hard failure whatever build it came from.  A dead site is
        # work still to do; a disagreement is the thing this project exists to
        # not have.  Finding 192 records that instrumentation costs nothing
        # under these flags, and this is where that stops being true.
        #
        sys.exit("  %d instrumented test(s) disagree with the blob: %s\n"
                 "  See finding 192."
                 % (len(bad), " ".join(os.path.basename(b) for b in bad)))


def gcov_lines(src):
    """(count, lineno, text) per line, count None for non-executable."""
    objdir = os.path.join(BUILD, os.path.dirname(src))
    if not glob.glob(os.path.join(objdir, os.path.basename(src)[:-2] + ".gcda")):
        return None
    out = subprocess.run(["gcov", "-t", "-o", objdir, src],
                         capture_output=True, text=True).stdout
    rows = []
    for line in out.split("\n"):
        p = line.split(":", 2)
        if len(p) < 3:
            continue
        c = p[0].strip()
        rows.append((None if c == "-" else c, p[1].strip(), p[2]))
    return rows


def main():
    if "--no-build" not in sys.argv:
        run(build())
    elif not glob.glob("%s/test/t_*" % BUILD):
        sys.exit("no %s tree to reuse -- drop --no-build" % BUILD)
    else:
        run(sorted(glob.glob("%s/test/t_*" % BUILD)))

    summary = "--summary" in sys.argv
    worst = []
    sites = dead = ex = tot = 0
    for src in sorted(glob.glob("src/**/*.c", recursive=True)):
        rows = gcov_lines(src)
        if rows is None:
            continue
        fn, out = None, []
        for c, no, text in rows:
            m = re.match(r"^([A-Za-z_]\w*)\s*\(", text)
            if m:
                fn = m.group(1)
            if c is not None:
                tot += 1
                if c not in ("#####", "====="):
                    ex += 1
            if "dsplibs_debug_printf" in text and "define" not in text:
                sites += 1
                if c in ("#####", "====="):
                    out.append((no, fn, text.strip()[:58]))
        if out:
            dead += len(out)
            if not summary:
                print("%s -- %d of the file's debug sites never execute"
                      % (src, len(out)))
                for no, f, text in out:
                    print("    %-5s %-24s %s" % (no, f or "?", text))
                print()
            else:
                worst.append((len(out), src))
        if "--lines" in sys.argv and not summary:
            l = sum(1 for c, _, _ in rows if c is not None)
            e = sum(1 for c, _, _ in rows if c not in (None, "#####", "====="))
            if l:
                print("    lines %5.1f%%  %s (%d/%d)" % (100.0 * e / l, src, e, l))

    if summary:
        # The count is the tracked number; the files are so a rise can be
        # placed without re-running the whole thing.
        print("debug sites: %d of %d never execute  (%s)"
              % (dead, sites, ", ".join("%s %d" % (os.path.basename(s), n)
                                        for n, s in sorted(worst, reverse=True))))
        print("             suite line coverage over src/ %.1f%% (%d/%d)"
              % (100.0 * ex / tot if tot else 0, ex, tot))
    else:
        print("  %d of %d dsplibs_debug_printf call sites never execute"
              % (dead, sites))
        print("  suite line coverage over src/: %d of %d = %.1f%%"
              % (ex, tot, 100.0 * ex / tot if tot else 0))
    #
    # Deliberately does not exit non-zero on a dead site.  Task #50 is the work
    # of retiring these, and a gate that fails from the first run is a gate
    # somebody turns off.  Compare the count against the last one instead.
    #
    return 0


if __name__ == "__main__":
    sys.exit(main())
