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
trust it: finding F154 worked out by hand that 3 of `CALLPROG_Progress`'s sites
were verified and the rest were not, and this reports 3 live sites in
Callprog.c and 30 dead, having been told nothing.

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
See finding F192.  Worth re-checking if the flags ever change.

    tools/debugcov.py                 build, run, report
    tools/debugcov.py --no-build      reuse an existing build-cov tree
    tools/debugcov.py --summary       the two counts only, for `make phase`
    tools/debugcov.py --lines         whole-file line coverage as well
    tools/debugcov.py --deviations    per-site coverage of docs/deviations.md

THE DEVIATION PASS (--deviations), AND WHY IT LIVES HERE

`tools/devaudit.py` asks a NECESSARY condition of each register entry: does a
compiled test object reference the `ref_` alias of the function the entry
names?  A "no" is conclusive and a "yes" is only an invitation to look, because
a linked function is not a fired branch.  This pass asks the stronger question
on the SAME instrumented tree that is already built for the debug sites: did
the line at the deviation's own site EXECUTE, and where the site is a guard,
which way did the branch go?

It lives in this file rather than in a new tool for two reasons.  The
instrumented build is the expensive part and it is already here, so the marginal
cost is a second `gcov` over the ~20 files a deviation names.  And `make phase`
already runs this script, so wiring the number into the gate needs no Makefile
edit -- and the Makefile is inside `tools/mutsnap.py`'s closure, where an edit
restales all 66 mutation suites for a report that changes no behaviour.

HOW A SITE IS IDENTIFIED, and why the obvious regex is wrong

`src/` already carries prose references of the form "see D26", "Registered as
D30", "/* D8 */".  A bare `\bD[0-9]+\b` sweep of `src/` and `include/` returns
129 hits and MOST OF THEM ARE NOT DEVIATIONS:

  - `D0`/`D1`/`D2` in a header are Itanium-ABI destructor variants.  Every
    `NOT POLYMORPHIC` note in `include/dsplib/` names them, and `D1` and `D2`
    are also real register entries.
  - `D8` in `src/dsp/FloatARMA.cpp` is the x87 opcode byte, in the comment that
    records objdump's FSUBP/FSUBRP swap.  `D8` is also a real register entry.

Neither is caught by checking the number against the register's id set, because
both collide with ids that exist.  The filter that works is CROSS-CHECKING THE
FILE: a tag counts as a site only when it appears in a file THE ENTRY ITSELF
NAMES, in its `**Where:**` or `**Module**` line.  `FloatARMA.cpp` is not D8's
module (`b103fp.c` is), so the opcode drops out; `include/` is never a module,
so the destructor notes drop out.  A tag in some other file is reported as a
CROSS-REFERENCE and measures nothing -- `fpm_phasor.c` mentioning D1 is prose
about `fpm_sqrt.c`.

WHAT THE RESULT MEANS, IN BOTH DIRECTIONS

Zero coverage is the RIGHT answer for a whole class of entries: one that says a
path is unreachable should show a dead site, and one that says the path fires
in ordinary operation should not.  So this prints the count and does not judge
it -- the judgement is against what the entry CLAIMS, which is prose.  Two
off-diagonals are the valuable output:

    entry says unreachable + branch taken   ->  THE ENTRY IS WRONG
    entry says fires normally + site dead   ->  a gap in the test domain

A tag sitting in a file-header comment with no executable line before it is
reported as `header` and NOT as covered: the next executable line after it is
the first function's entry, which everything executes, and reading that as
"measured" is exactly the false pass this whole pass exists to remove.
"""

import glob
import json
import os
import re
import subprocess
import sys

BUILD = "build-cov"
# The denominators, for `make phase`'s closing line to quote.  Under BUILD/ so
# it is gitignored and so a `make clean` takes it with the data it describes.
COUNTS = os.path.join(BUILD, "measured.txt")
COV = ("-Wall -Wextra -Wno-unused-parameter -g -O2 -Iinclude -MMD -MP "
       "--coverage")
LD = "-no-pie -Wl,-z,noexecstack,-z,notext --coverage"


def tests():
    #
    # BOTH LISTS.  `CXXTESTS` is a separate variable in the Makefile because
    # its members compile with $(CXX), and reading only `TESTS` here meant the
    # instrumented tree never built or ran them -- so the sites a C++ test is
    # the only driver for counted as dead, and the number this whole target
    # exists to track would have RISEN for a batch of sites that are in fact
    # driven.
    #
    # ASK MAKE, do not grep the Makefile.  Both lists became wildcard
    # expressions so that parallel agents stop colliding on one line, at
    # which point a regex for `^TESTS\s*:=(.*)$` returns the expression as
    # text and this tried to build a target named `$(basename`.
    return subprocess.run(["make", "-s", "print-TESTS", "print-CXXTESTS"],
                          capture_output=True, text=True).stdout.split()


def build():
    targets = ["%s/test/%s" % (BUILD, t) for t in tests()]
    r = subprocess.run(["make", "-j8", "BUILD=" + BUILD, "CFLAGS=" + COV,
                        "LDFLAGS=" + LD] + targets,
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("instrumented build failed:\n" + r.stdout[-4000:] + r.stderr[-4000:])
    return targets


def gcc_diverges():
    """The tests tools/gccdiverge.json excuses under THIS compiler.

    Read here for the same reason the test runner reads it: a site where
    modern GCC provably cannot reproduce the object fails identically in the
    instrumented build, and this target would otherwise be the one thing left
    holding the whole tree to a standard the compiler cannot meet.  The
    register is empty unless something is declared in it, and `make period`
    -- which has no allow-list -- is what actually decides.
    """
    try:
        with open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "gccdiverge.json")) as f:
            return set(json.load(f))
    except OSError:
        return set()


def run(targets):
    for f in glob.glob("%s/**/*.gcda" % BUILD, recursive=True):
        os.unlink(f)                        # counts must not carry over
    excused = gcc_diverges()
    bad = [t for t in targets
           if subprocess.run([t], capture_output=True).returncode != 0
           and os.path.basename(t) not in excused]
    if bad:
        #
        # THIS one is fatal, and it is the only thing here that is -- for the
        # ordinary reason, not a special one: the goal is a replacement that
        # behaves identically to the blob, so ANY test disagreeing with the
        # blob is a hard failure whatever build it came from.  A dead site is
        # work still to do; a disagreement is the thing this project exists to
        # not have.  Finding F192 records that instrumentation costs nothing
        # under these flags, and this is where that stops being true.
        #
        sys.exit("  %d instrumented test(s) disagree with the blob: %s\n"
                 "  See finding F192."
                 % (len(bad), " ".join(os.path.basename(b) for b in bad)))


#
# WHERE THE INSTRUMENTED OBJECTS LIVE, and why this is a PROBE and not a path.
#
# The differential tier links $(OBJ_REPRO) since task #164, "build: split the
# object tree so the DEFAULT build carries our fixes", so the objects the suite
# actually runs -- and the .gcda written
# beside them -- are `build-cov/repro/dsp/foo.o` for `src/dsp/foo.c`, with the
# leading `src/` stripped exactly as CXXOBJ64 strips it.  Before the split they
# were `build-cov/src/dsp/foo.o`.  This file went on looking only at the old
# location and every lookup returned None, so the whole tool measured NOTHING
# and reported 0.0% (0/0) and "0 of 0" at exit 0.
#
# That is finding F2400 exactly over again -- a build moved its output and a
# detector kept reading the old path -- so the answer is both halves of 2401's
# ruling: probe both layouts and take whichever HAS the data, and make a zero
# denominator fatal below so the next move cannot be silent either.
#
# The order matters only for a tree carrying both: `repro` is the one the test
# binaries link, so it is the one whose counts describe what ran.
#
def objdir(src):
    """The build-cov directory holding src's .gcda, or None if there is none."""
    for d in objdirs(src):
        if glob.glob(os.path.join(d, stem_of(src) + ".gcda")):
            return d
    return None


def objdirs(src):
    """Every layout this tool knows about, newest first -- for the diagnostic."""
    rel = os.path.dirname(src)
    return [os.path.normpath(os.path.join(BUILD, "repro",
                                          os.path.relpath(rel, "src"))),
            os.path.join(BUILD, rel)]


def stem_of(src):
    return os.path.splitext(os.path.basename(src))[0]


#
# A DETECTOR MUST REPORT ITS DENOMINATOR (finding F2401), and a denominator of
# zero is not a pass.  Every number this tool prints is a ratio over what gcov
# gave it, and "everything is covered" and "nothing was measured" render
# IDENTICALLY -- 0 of 0, 0 dead, 0.0% -- while the second is the tool being
# broken.  `extcheck` printed "(none)" through four broken versions for want of
# this (134); both codegen aids compared zero symbols and reported a clean tree
# at exit 0 for want of it again (2400, 2401).  This is the third instance.
#
def zero_denominator(what):
    sample = (sorted(glob.glob("src/**/*.c", recursive=True)) or ["src/x.c"])[0]
    found = glob.glob("%s/**/*.gcda" % BUILD, recursive=True)
    return ("  %s.\n"
            "  THE DENOMINATOR IS ZERO, which is not a clean sheet -- it is\n"
            "  this tool measuring nothing and saying so in the same words it\n"
            "  would use for a perfect score, which is why it refuses.\n"
            "  Looked for %s.gcda in: %s\n"
            "  %d .gcda file(s) exist anywhere under %s/.\n"
            "  If the build tree moved again, teach objdirs() where it went;\n"
            "  if it was never built, drop --no-build.  Findings F134, F2400, F2401."
            % (what, stem_of(sample), ", ".join(objdirs(sample)),
               len(found), BUILD))


def gcov_lines(src):
    """(count, lineno, text) per line, count None for non-executable."""
    d = objdir(src)
    if d is None:
        return None
    out = subprocess.run(["gcov", "-t", "-o", d, src],
                         capture_output=True, text=True).stdout
    rows = []
    for line in out.split("\n"):
        p = line.split(":", 2)
        if len(p) < 3:
            continue
        c = p[0].strip()
        rows.append((None if c == "-" else c, p[1].strip(), p[2]))
    return rows


REG = "docs/deviations.md"

# A prose reference, not an ABI destructor variant and not an x87 opcode: no
# backtick or word character either side, and not followed by `(`, which is how
# `include/dsplib/Scrambler.h` writes `D1()`.  The file cross-check in
# dev_sites() is what actually does the work; this only keeps the candidate set
# small enough to cross-check.
TAGRE = re.compile(r"(?<![`\w])D(\d{1,2})(?![\w`(])")


def dev_entries(path=None):
    """{id: {head, files, claim}} for every `## D<n>` block in the register.

    `path=None` and not `path=REG`: a default argument binds at def time, so
    the module global could be repointed and this would go on reading the
    original file -- which is how the empty-register guard below came to pass
    its own test while doing nothing.
    """
    out = {}
    for block in re.split(r"^## ", open(path or REG).read(), flags=re.M)[1:]:
        head = block.split("\n", 1)[0].strip()
        m = re.match(r"(D\d+)", head)
        if not m:
            continue
        body = block.split("\n", 1)[1] if "\n" in block else ""
        # Both spellings.  The older entries carry `**Module** `src/x.c``; the
        # V.34 ones onwards carry `**Where:** `src/x.c`, `func``, which is
        # better evidence because it names the function too.
        where = re.search(r"\*\*(?:Where:|Modules?)\*\*(.*)", body)
        line = where.group(1).strip() if where else ""
        out[m.group(1)] = dict(
            head=head, line=line,
            files=re.findall(r"`(src/[^`]+\.(?:c|cpp))`", line),
            retracted=("❌" in head or "RETRACTED" in head),
            unmeasured=("unmeasured" in body or "not measured" in body))
    return out


def dev_sites(entries):
    """[(id, file, lineno, text, is_site)] -- is_site false means cross-ref."""
    hits = []
    for f in sorted(glob.glob("src/**/*.c", recursive=True)
                    + glob.glob("src/**/*.cpp", recursive=True)):
        for n, line in enumerate(open(f, errors="replace"), 1):
            for m in TAGRE.finditer(line):
                did = "D" + m.group(1)
                if did in entries:
                    hits.append((did, f, n, line.strip()[:70],
                                 f in entries[did]["files"]))
    return hits


def gcov_marked(src):
    """{lineno: (count, [branch counts])} with -b, or None if never compiled."""
    d = objdir(src)
    if d is None:
        return None
    out = subprocess.run(["gcov", "-b", "-t", "-o", d, src],
                         capture_output=True, text=True).stdout
    rows, last = {}, None
    for line in out.split("\n"):
        #
        # BRANCH LINES BELONG TO THE LINE ABOVE THEM.  gcov -b emits
        # "branch  0 taken 5 (fallthrough)" AFTER the source line it describes,
        # with no colons, so the ordinary parse skips it and the arc data --
        # the only thing that separates "the guard ran" from "the guarded arm
        # ran" -- is silently lost.  That distinction is the point of the -b.
        #
        b = re.match(r"\s*branch\s+\d+\s+(taken\s+(\d+)|never executed)", line)
        if b and last is not None:
            rows[last][1].append(int(b.group(2)) if b.group(2) else 0)
            continue
        p = line.split(":", 2)
        if len(p) < 3:
            continue
        c, no = p[0].strip(), p[1].strip()
        if not no.isdigit():
            continue
        last = int(no)
        rows[last] = (None if c == "-" else c, [], p[2])
    return rows


def deviation_pass(quiet=False):
    """(full, gap, header, nogcda, entries_with_a_site, folded)."""
    if not os.path.exists(REG):
        sys.exit("no %s -- nothing to measure" % REG)
    entries = dev_entries()
    if not entries:
        sys.exit("%s parsed to zero entries -- the heading format has changed"
                 % REG)
    hits = dev_sites(entries)
    #
    # THE VACUOUS GUARD.  An empty tag set and a fully-covered tag set both
    # print "0 dead", and the difference is the whole result.  `src/` carries
    # these references today; if a rename or a comment sweep removes them this
    # must fail loudly rather than report a clean sheet.
    #
    if not [h for h in hits if h[4]]:
        sys.exit("no deviation site tags found in src/ -- either the prose "
                 "convention changed or TAGRE no longer matches it")

    cache, rows = {}, []
    for did, f, n, text, is_site in hits:
        if not is_site:
            rows.append((did, f, n, "xref", None, text))
            continue
        if f not in cache:
            cache[f] = gcov_marked(f)
        g = cache[f]
        if g is None:
            rows.append((did, f, n, "nogcda", None, text))
            continue
        if not [k for k in g if k < n and g[k][0] is not None]:
            # File-header comment: the next executable line is some function's
            # entry, which everything runs.  Not a measurement.
            rows.append((did, f, n, "header", None, text))
            continue
        rows.append((did, f, n) + region(g, n) + (text,))

    kinds = {}
    for r in rows:
        kinds[r[3]] = kinds.get(r[3], 0) + 1
    #
    # THE OTHER VACUOUS GUARD, and the one that actually fired.  The tag sweep
    # above proves the PROSE is still there; this proves the COVERAGE is.  With
    # every file's .gcda missing, every row is `nogcda`, and the summary line
    # below -- which does not print that column at all -- reads "0 of 0
    # anchored ... over 0 entries" and the phase boundary calls it OK.
    #
    if not sum(kinds.get(k, 0) for k in ("full", "gap", "folded", "header")):
        sys.exit(zero_denominator(
            "the deviation pass anchored %d site tag(s) in src/ and got gcov "
            "data for none of them (%d with no .gcda)"
            % (len([h for h in hits if h[4]]), kinds.get("nogcda", 0))))
    named = set(d for d, _, _, k, _, _ in rows if k in ("full", "gap", "folded"))
    if not quiet:
        for did in sorted(entries, key=lambda d: int(d[1:])):
            e = entries[did]
            mine = [r for r in rows if r[0] == did and r[3] != "xref"]
            if not mine and not e["files"]:
                continue                    # no file named: devaudit's ground
            print("%-5s %-10s %s" % (did, "unmeasured" if e["unmeasured"]
                                     else ("retracted" if e["retracted"]
                                           else "claimed"), e["head"][:58]))
            if not mine:
                print("      %-6s %s -- entry names a file but src/ carries "
                      "no `D%s` reference to anchor it"
                      % ("notag", ", ".join(e["files"]), did[1:]))
            for _, f, n, kind, det, text in mine:
                if det is None:
                    print("      %-6s %s:%d  | %s" % (kind, f, n, text))
                    continue
                lo, hi, deadl, nb = det
                print("      %-6s %s:%d  fn lines %d-%d: %d never execute, "
                      "%d branch arm(s) never taken"
                      % (kind, f, n, lo, hi, len(deadl), len(nb)))
                for k, t in (deadl + nb)[:4]:
                    print("             %5d  %s" % (k, t))
        print()
    return (kinds.get("full", 0), kinds.get("gap", 0), kinds.get("header", 0),
            kinds.get("nogcda", 0), len(named), kinds.get("folded", 0))


def region(g, n):
    """(kind, (lo, hi, dead lines, untaken branches, ...)) around line n.

    The enclosing function, delimited by a `}` in column 1 -- which is what
    this tree's style puts at the end of every function and nowhere else.
    Reporting the NEAREST EXECUTABLE LINE instead was the first cut and it is
    not worth having: the prose tag usually sits in the comment ABOVE the
    function, so the nearest line is the function's own entry, whose count
    says only that the function was called.  That is exactly the necessary
    condition `devaudit.py` already gives.  What a deviation entry claims is
    almost always about an ARM -- "nothing can select it", "cannot be taken",
    "the sweep reaches it" -- so the answer has to be about arcs inside the
    function, not about the function.
    """
    ends = sorted(k for k in g if g[k][2].startswith("}"))
    lo = max([k for k in ends if k < n] or [0]) + 1
    hi = min([k for k in ends if k >= n] or [max(g)])
    dead = [(k, g[k][2].strip()[:56]) for k in sorted(g)
            if lo <= k <= hi and g[k][0] in ("#####", "=====")]
    untaken = [(k, "branch never taken: " + g[k][2].strip()[:36])
               for k in sorted(g)
               if lo <= k <= hi and g[k][1] and 0 in g[k][1]]
    fold = [(k, "compiler folded: " + g[k][2].strip()[:40])
            for k in sorted(g) if lo <= k <= hi and folded(g, k)]
    return ("folded" if fold else ("gap" if dead or untaken else "full"),
            (lo, hi, dead + fold, untaken))


#
# A STATEMENT GCOV MARKS NON-EXECUTABLE.  This is the strongest thing the pass
# produces and it exists because the obvious detector misses it entirely.
#
# D36 and D53 both claim a branch "cannot be taken".  Both are right, and
# NEITHER shows up as a dead line or as an untaken arc: GCC proved the guard
# unsatisfiable and emitted no code at all for its body, so gcov marks those
# lines `-` -- the same mark it gives a comment.  A detector looking for
# `#####` or a zero arc reports the whole function as fully covered, which is
# the exact opposite of the truth.  D53's entry records the same observation
# about `debugcov`'s dead-SITE count and is the reason this is not a guess.
#
# So: a DIAGNOSTIC CALL SITE carrying no execution count, with an executed line
# just above it.  The second clause keeps this off every comment and
# declaration in the file, all of which are `-` too; the first is what keeps it
# honest.  `return` and `goto` were tried as extra triggers and dropped: gcov
# also marks a line `-` when the compiler MERGED it into another block, and
# `preempindex`'s last `return i;` and `probeselect`'s last `return;` are both
# that -- reachable, folded into the epilogue, and indistinguishable from a
# proved-dead one by the mark alone.  A `dsplibs_debug_printf` is never merged
# away: it is a call, and a call that emitted no code did not survive the
# optimiser at all.
#
# This is also the category `debugcov`'s own dead-SITE count cannot hold, which
# D53's entry states from the other direction: a site gcov marks non-executable
# is neither live nor "executed zero times", so it silently leaves both
# columns.  The deviation pass is where it now lands.
#
# Read a `folded` result as A PROOF OF UNREACHABILITY AT THE C LEVEL, reached
# mechanically and agreeing with the object's instruction order.  It is the one
# place this pass turns a necessary condition into a sufficient one.
#
# It does NOT distinguish "the compiler proved it dead" from "this arm was not
# compiled into this build", which is what `#ifdef DSPLIB_REPRODUCE_BUGS` in
# `fpm_div.c` produces.  Only one file in the tree has that, and its tags are
# header tags, so the two do not collide today.
#
FOLDRE = re.compile(r"dsplibs_debug_printf\(")


def folded(g, k):
    if g[k][0] is not None or not FOLDRE.search(g[k][2]):
        return False
    for j in range(k - 1, max(k - 7, 0), -1):
        if j in g and g[j][0] is not None:
            return g[j][0] not in ("#####", "=====")
    return False


def main():
    if "--deviations" in sys.argv and "--no-build" in sys.argv:
        full, gap, hdr, no, n, fold = deviation_pass()
        print("deviation sites: %d anchored in a function every line and arc "
              "of which runs,\n                 %d in one with dead code or "
              "an untaken arm, %d header-only, %d no data;\n"
              "                 %d folded (compiler-proved dead), %d entries "
              "carry an anchored site" % (full, gap, hdr, no, fold, n))
        return 0
    if "--no-build" not in sys.argv:
        run(build())
    elif not glob.glob("%s/test/t_*" % BUILD):
        sys.exit("no %s tree to reuse -- drop --no-build" % BUILD)
    else:
        run(sorted(glob.glob("%s/test/t_*" % BUILD)))

    summary = "--summary" in sys.argv
    #
    # REMOVED BEFORE ANYTHING IS MEASURED, so the phase boundary cannot read a
    # previous run's numbers: a stale denominator is the same lie as a missing
    # one, told more convincingly.  Here rather than at the top of main(),
    # because `--deviations --no-build` -- the command the summary line tells
    # people to run -- returns above this and has no business deleting it.
    #
    if summary:
        try:
            os.unlink(COUNTS)
        except OSError:
            pass
    worst = []
    sites = dead = ex = tot = files = 0
    #
    # .cpp AS WELL AS .c, for the reason debugaudit.py's own glob now gives:
    # a C++ translation unit's diagnostics were counted by neither tool, and
    # FloatIIR.cpp printing nothing is what kept that invisible.
    #
    for src in sorted(glob.glob("src/**/*.c", recursive=True)
                      + glob.glob("src/**/*.cpp", recursive=True)):
        rows = gcov_lines(src)
        if rows is None:
            continue
        files += 1
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

    #
    # ZERO IS NOT A SCORE.  `tot` is every executable line gcov reported and
    # `sites` every diagnostic call site it saw; both are zero exactly when the
    # gcov data was not found, and the lines below would then print 0.0% (0/0)
    # and "0 of 0 never execute" -- indistinguishable from a tree with no dead
    # sites, which is what this tool exists to report on.  Fatal, per 2401.
    #
    if tot == 0 or sites == 0:
        sys.exit(zero_denominator(
            "%d source file(s) yielded gcov data: %d executable line(s) and "
            "%d debug site(s)" % (files, tot, sites)))

    if summary:
        # The count is the tracked number; the files are so a rise can be
        # placed without re-running the whole thing.  EVERY LINE HERE CARRIES
        # ITS DENOMINATOR, including the file count -- 2401's ruling, and the
        # reason this tool's silence lasted as long as it did.
        print("debug sites: %d of %d never execute, over %d file(s) with "
              "coverage data  (%s)"
              % (dead, sites, files,
                 ", ".join("%s %d" % (os.path.basename(s), n)
                           for n, s in sorted(worst, reverse=True))))
        print("             suite line coverage over src/ %.1f%% (%d/%d)"
              % (100.0 * ex / tot, ex, tot))
        dr, dd, dh, dn, de, df = deviation_pass(quiet=True)
        #
        # `%d with no coverage data` IS THE COLUMN THAT WAS MISSING.  The
        # --deviations report has always printed it; this line dropped it, so a
        # pass where every site was unmeasured rendered as "0 of 0" and read as
        # a clean sheet.  It is the same defect as the guard above, one level up.
        #
        print("deviation sites: %d of %d anchored in a fully-covered function, %d"
              " with dead code\n                 or an untaken arm, %d"
              " compiler-folded, %d header-only, %d with no coverage data,\n"
              "                 over %d entries"
              "  -- tools/debugcov.py --deviations --no-build"
              % (dr, dr + dd + df, dd, df, dh, dn, de))
        #
        # THE DENOMINATORS, WHERE THE PHASE BOUNDARY CAN READ THEM.  `make
        # phase`'s closing line is the one a human reads to call the tree
        # green and it carried no numbers at all; it now prints this and
        # REFUSES if the file is missing, so the boundary cannot pronounce on
        # a tier that measured nothing.  Written last, after every guard.
        #
        with open(COUNTS, "w") as f:
            f.write("measured: %d/%d src/ lines over %d file(s), %d debug "
                    "sites, %d anchored deviation sites\n"
                    % (ex, tot, files, sites, dr + dd + df))
    else:
        print("  %d of %d dsplibs_debug_printf call sites never execute, over "
              "%d file(s) with coverage data" % (dead, sites, files))
        print("  suite line coverage over src/: %d of %d = %.1f%%"
              % (ex, tot, 100.0 * ex / tot))
    #
    # Deliberately does not exit non-zero on a dead site.  Task #50 is the work
    # of retiring these, and a gate that fails from the first run is a gate
    # somebody turns off.  Compare the count against the last one instead.
    #
    return 0


if __name__ == "__main__":
    sys.exit(main())
