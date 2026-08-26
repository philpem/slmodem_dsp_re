#!/usr/bin/env python3
"""Compare our object code against the blob's, function by function.

The differential tier proves the two BEHAVE the same.  This asks the harder
question: does the same compiler, given our source, emit what the original
compiler emitted?  A function that comes out byte-identical is evidence about
the SOURCE that no amount of black-box testing can give -- it says the
expression shape, the operand order and the control flow were recovered, not
merely something equivalent to them.

Build the objects first, with the period toolchain (see Dockerfile.exact here
and findings F606 and F2200):

    docker build --platform linux/386 -f tools/toolchain/Dockerfile.exact \
                 -t dsplibs-tc342 tools/toolchain
    make tc                              # writes build/tc_out/*.o
    tools/toolchain/compare.py

THE COMPILER IS GCC 3.4.2 ITSELF, bootstrapped from the GNU tarball.  Until
finding F2200 it was Debian sarge's `3.4.4 20050314 (prerelease)`, two point
releases and one vendor's patch stack away from the `3.4.2` the blob names,
and every number below had been measured with it.  The point release reaches
codegen: 18 of 183 translation units differ between the two, and the exact
compiler matches 330 of 924 symbols where 3.4.4 matches 324 -- SIX GAINED,
NONE LOST.  Every run now prints both `.comment` strings so the reference and
the measurement can be read together.

AND THE VENDOR'S PATCH STACK NOW HAS A NUMBER TOO.  `dsplibs-tc342-gentoo`
(Dockerfile.gentoo) is Gentoo's own `gcc-3.4.2-r2` built from its ebuild
inside stage3-x86-2005.0, and it is the only compiler here whose `.comment`
matches the blob's byte for byte.  It changes NOTHING measurable: 182 of the
183 objects are byte-identical to stock 3.4.2's, the match is 334 either way
with none gained and none lost, and the one difference is a schedule
permutation in `DTMF_MTD_detect`.  So the two `.comment` lines below being
equal is a property of the IMAGE and not of the numbers -- which is why the
default stays the stock one, buildable from the network alone.  Finding F2500.

THE FLAGS ARE NOT GUESSES.  Each was read out of the object:

    -march=i386             no cmov and no fcomi anywhere in 1.2 MB, and float
                            compares go the long way with fnstsw/sahf.  This
                            bounds the INSTRUCTION SET only
    -mtune=i686             ...and the SCHEDULING is separately i686, which the
                            object cannot show directly but the match rate can:
                            i686 tuning takes byte-identical functions from 30
                            to 82.  Every i686-family tune value (pentiumpro,
                            pentium2, pentium3, and the period spelling
                            `-mcpu=i686`) gives the identical result
    -mfpmath=387            follows from the above
    -mno-ieee-fp            the object's float compares are ORDERED.  Across
                            1.2 MB: 406 fcom/fcoms/fcomp/fcomps/fcompp/fcoml/
                            fcompl against FOUR fucom, and all four of those
                            are inside libm's `pow`, which is not our code.
                            Under the default -mieee-fp this compiler emits
                            fucom for EVERY comparison regardless of the source
                            operator -- `>=`, `>`, `!(<)` and `==` all give
                            fucompp -- so the object cannot have been built
                            with it.  Same argument as -march=i386's: a
                            mnemonic that is absent from 1.2 MB bounds the
                            flag.  Worth +2 identical on its own, but the
                            effect that matters is that EVERY float comparison
                            in EVERY function used to read as a mismatch.
                            Finding F1990
    -fomit-frame-pointer    no push %ebp / mov %esp,%ebp in the object's
                            prologues.  GCC 3.4 does NOT imply this at -O2
    -maccumulate-outgoing-args
                            the object pre-allocates its outgoing argument area
                            and fills it with `mov %reg,(%esp)`; without the
                            flag GCC pushes
    (no -fPIC/-fPIE)        not one get_pc_thunk in the object
    (no -fstack-protector)  no __guard, no __stack_smash_handler

    -frename-registers      a post-reload pass GCC 3.4 enables at -O3.  Found
                            by bisecting Agc<float>::reset, which it makes
                            byte-identical; tree-wide it accounts for ALL of
                            -O3's advantage, 83 identical -> 92.  The rest of
                            -O3 adds 17 KB of our code and no matches at all,
                            so -O3 itself is disfavoured.  Finding F616.

`-O3` IS THE LEVEL, and this paragraph used to say the opposite.  It used to
read that `-O2` was the level and that the only open question was whether the
author passed `-frename-registers` explicitly or passed `-O3` and got it while
the rest of -O3 happened not to bite.  Measured on the whole tree with the
level as the only variable: `-O2` 313 identical, `-O2 -finline-functions` 314,
`-O3` 324 -- and the `-O3` set GAINS 15 and loses 4 rather than swapping, with
byte coverage 72.2% -> 80.0% and no new overshoot.  `make period` is green at
all three.  Finding F616 measured the opposite when this tree matched 92 of
365, where `-finline-functions` had almost nothing to inline across.
`-frename-registers` stays spelled out although `-O3` implies it, because
616's evidence for it is independent.  Finding F2155.

RE-MEASURED ON GCC 3.4.2 ITSELF, because 2155 and 1990 were both taken on the
wrong point release, and 2155 says of itself that it is a match-rate argument
a better hypothesis could overturn.  A better compiler is a better hypothesis.
It does not overturn either of them; it widens both:

    compiler   flags                     identical  same_size  of blob's bytes
    3.4.4      -O2  -mno-ieee-fp             313         67        72.2%
    3.4.4      -O3  -mieee-fp                322         70        80.1%
    3.4.4      -O3  -mno-ieee-fp             324         69        80.0%
    3.4.2      -O2  -mno-ieee-fp             315         65        72.2%
    3.4.2      -O3  -mieee-fp                328         68        80.2%
    3.4.2      -O3  -mno-ieee-fp (the set)   330         67        80.1%

`-O3` over `-O2` on the exact compiler gains 19 and loses 4, where the 3.4.4
reading was gains 15 loses 4 -- and the four lost are the SAME four 2155 named.
`-mno-ieee-fp` gains 2 and loses none, exactly as it did on 3.4.4; its
mechanism argument, the half that does not depend on match rate, reproduces
unchanged: 3.4.2 under the default `-mieee-fp` emits `fucompp` for `a>=b`,
`a>b`, `!(a<b)`, `a==b` and the `double` form alike, and `fcomps`/`fcompl` for
every one of them with the flag.  Finding F2200.

WHAT THE NUMBERS MEAN, and do not mean.  A size mismatch is not a defect: our
source is not the original's source, and a function we wrote as one loop that
the original wrote as two will differ while behaving identically.  Read this as
a similarity gradient, not a pass/fail.  The differential tests remain the only
thing that decides correctness.

THE TOTAL-BYTES PERCENTAGE IS THE WEAK NUMBER.  It moves when we emit more
code, not only when we emit more of the RIGHT code -- we currently undershoot,
so anything that inlines harder closes the gap arithmetically, and a build that
inlined wildly could pass 100% while matching nothing.  The byte-identical
count cannot be gamed that way.  Quote that; quote the percentage only
alongside it.  Finding F612.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys

#
# THE RATCHET, and why it is a ratchet and not a gate.
#
# 100% is not the target and never will be: our source is not the original's
# source, so a function we wrote as one loop where the author wrote two will
# differ for ever while behaving identically.  A "must match" gate would fail
# on every file in the tree and teach everyone to ignore it.
#
# What IS meaningful is the direction.  A number that only ever goes up turns
# the comparison into a progress metric and, more usefully, catches the case
# where a change makes the reconstruction LESS like the original while all the
# differential tests still pass -- which is exactly the kind of regression this
# project has no other way to see.
#
RATCHET = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "ratchet.json")

#
# TRANSLATION UNITS WE DELIBERATELY SPLIT, and the original did not.
#
# A per-symbol size comparison across an inlining boundary measures the
# reconstruction's FACTORING, not its completeness -- the same trap
# debugaudit.py fell into (finding F605).  Where we broke one of the original's
# functions into static helpers, the helpers have no blob symbol, so their
# bytes are counted against neither side and the blob's function shows the
# whole difference as missing.  The per-object rollup below fixes that case on
# its own.
#
# It cannot fix a split across FILES, because there is nothing in either object
# to say the two belong together.  Those are declared here.  Finding F610.
#
TU_GROUPS = (
    ("src/v8/v8handshak.c", "src/v8/v8hsrx.c"),
)

def _default_blob():
    """`ref/slmodemd/dsplibs.o` is relative to the MAIN tree, not to a worktree.

    From `.claude/worktrees/<name>` that literal path resolves inside
    `.claude/worktrees/`, where there is no blob -- and `nm` on a missing file
    exits non-zero with empty stdout, which `sizes()` cannot tell from a file
    with no symbols.  Every count is then computed against nothing.  Finding
    F3121.  `git rev-parse --git-common-dir` is how the `Makefile` already
    resolves this, so it is the tree's existing answer rather than a new one.
    """
    literal = "ref/slmodemd/dsplibs.o"
    if os.path.exists(literal):
        return literal
    try:
        common = subprocess.run(["git", "rev-parse", "--git-common-dir"],
                                capture_output=True, text=True,
                                check=True).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return literal
    main = os.path.dirname(os.path.abspath(common))
    found = os.path.join(os.path.dirname(main), "slmodemd", "dsplibs.o")
    return found if os.path.exists(found) else literal


BLOB = os.environ.get("BLOB") or _default_blob()
OURS = os.environ.get("TC_OUT", "build/tc_out")


def sizes(path):
    """symbol -> size, for anything with code in it."""
    out = subprocess.run(["nm", "--size-sort", "-S", path],
                         capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "TtWw":
            d[p[3]] = int(p[1], 16)
    return d


def comment(path):
    """Who built this object, out of its `.comment` section.

    THIS MEASUREMENT IS ONLY AS GOOD AS THE COMPILER THAT TOOK IT, and for a
    long time nothing printed which one that was: the container ran GCC 3.4.4
    (Debian sarge) while every flag conclusion in this file was written up as
    3.4.2, the version the blob names.  It cost +6 identical, and nobody could
    have seen it from the output.  Both strings now print on every run, so the
    reference and the measurement are side by side.  Finding F2200.
    """
    out = subprocess.run(["readelf", "-p", ".comment", path],
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        m = re.match(r"^\s*\[\s*[0-9a-f]+\]\s+(.*\S)", line)
        if m:
            return m.group(1)
    return "(no .comment)"


def mnemonics(path, sym):
    """The instruction mnemonic sequence, with addresses and operands dropped.

    Operands are dropped on purpose: a relocated address, a stack offset that
    differs by one slot and a register the allocator chose differently are all
    noise against the question being asked, which is whether the same
    instructions come out in the same order.
    """
    out = subprocess.run(
        ["objdump", "-d", "--disassemble=" + sym, "--no-show-raw-insn", path],
        capture_output=True, text=True).stdout
    return [m.group(1) for m in
            (re.match(r"^\s*[0-9a-f]+:\s+(\S+)", l) for l in out.splitlines())
            if m]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ratchet", action="store_true",
                    help="fail if fewer functions match than last time")
    ap.add_argument("--update", action="store_true",
                    help="record the current counts as the new floor")
    args = ap.parse_args()

    blob = sizes(BLOB)
    if not blob:
        sys.exit(
            "compare.py: NO SYMBOLS read from the blob at %s (file exists: %s).\n"
            "  Every count below would be computed against NOTHING and would\n"
            "  render as a clean zero -- and `--update` would write\n"
            "  {identical: 0, same_size: 0, compared: 0} into %s, destroying\n"
            "  the floor.  That has already happened once.\n"
            "  BLOB defaults to a path relative to the MAIN tree, so from a\n"
            "  worktree it must be given explicitly:\n"
            "      BLOB=/abs/path/to/dsplibs.o %s\n"
            "  A detector must report its denominator, and zero is not a score:\n"
            "  findings F2400, F2401, F3110 and F3121."
            % (BLOB, "yes" if os.path.exists(BLOB) else "no", RATCHET,
               " ".join(sys.argv)))
    ours = {}
    objs = sorted(glob.glob(os.path.join(OURS, "*.o")))
    for o in objs:
        for k, v in sizes(o).items():
            ours.setdefault(k, (v, o))
    if not ours:
        sys.exit("no objects in %s -- run `make tc` first" % OURS)

    print("  blob built by: %s" % comment(BLOB))
    print("  ours built by: %s" % comment(objs[0]))
    print("  objects      : %s\n" % OURS)

    common = sorted(k for k in ours if k in blob)
    if not common:
        sys.exit(
            "compare.py: the blob defines %d symbols and %s defines %d, and\n"
            "  they share NONE.  The comparison denominator is zero, which is\n"
            "  not a score -- it renders identically to a tree that matches\n"
            "  nothing.  Most likely BLOB and TC_OUT point at unrelated trees,\n"
            "  or the two were built for different targets.  Findings F2401\n"
            "  and 3121."
            % (len(blob), OURS, len(ours)))
    identical, samesize, rows = [], [], []
    tb = to = 0
    for k in common:
        b = blob[k]
        o, path = ours[k]
        tb += b
        to += o
        if b == o:
            if mnemonics(BLOB, k) == mnemonics(path, k):
                identical.append((b, k))
            else:
                samesize.append((b, k))
        rows.append((o - b, b, o, k))

    print("Comparing %d symbols the blob and this tree both have.\n" % len(common))
    print("  identical instruction sequences (mnemonics)    : %4d" % len(identical))
    print("  same size, different instructions             : %4d" % len(samesize))
    print("  different size                                : %4d"
          % (len(common) - len(identical) - len(samesize)))
    print("\n  total code: blob %d bytes, ours %d (%.1f%%)"
          % (tb, to, 100.0 * to / tb if tb else 0))

    if identical:
        print("\nIDENTICAL MNEMONIC SEQUENCES.  Operands are NOT compared -- two"
              "\nfunctions storing the same constants to different offsets in a"
              "\ndifferent order both read as `mov mov mov` and count here."
              "\nStrong evidence, but not byte equality.  Finding F615:")
        for b, k in sorted(identical, reverse=True):
            print("  %5d  %s" % (b, k))

    rows.sort()
    print("\nWhere we emit the least against the blob -- usually a helper the"
          "\noriginal inlined, or diagnostic sites we have not restored:")
    for delta, b, o, k in rows[:8]:
        print("  %+7d  blob %5d  ours %5d  %s" % (delta, b, o, k))
    print("\nWhere we emit the most:")
    for delta, b, o, k in rows[-5:]:
        print("  %+7d  blob %5d  ours %5d  %s" % (delta, b, o, k))

    #
    # PER OBJECT, which is the number to read before treating any single-symbol
    # gap as missing code.
    #
    def obj_text(path):
        """`.text` PLUS the linkonce sections -- the weak class templates put
        every member in one of those and would otherwise measure as zero."""
        out = subprocess.run(["size", "-A", path], capture_output=True,
                             text=True).stdout
        n = 0
        for line in out.splitlines():
            f = line.split()
            if len(f) >= 2 and (f[0] == ".text"
                                or f[0].startswith(".gnu.linkonce.t.")):
                n += int(f[1])
        return n

    # period.mk records object -> source; the underscore encoding is not
    # reversible (`dp_wrapper.c` would come back as `dp/wrapper.c`).
    manifest = {}
    try:
        for line in open(os.path.join(OURS, "tc_manifest.txt")):
            o, s = line.split()
            manifest[o] = s
    except OSError:
        pass

    def srcname(path):
        b = os.path.basename(path)
        return manifest.get(b, b[:-2].replace("_", "/"))

    # Every object, not only those with a symbol in common: a file we split out
    # of one of the original's functions has NO symbol the blob shares, so
    # keying on `common` would drop exactly the file that explains the gap.
    per = {}
    for path in sorted(glob.glob(os.path.join(OURS, "*.o"))):
        b = sum(blob[k] for k in sizes(path) if k in blob)
        per[srcname(path)] = [b, obj_text(path)]
    for group in TU_GROUPS:
        present = [g for g in group if g in per]
        if len(present) > 1:
            b = sum(per[g][0] for g in present)
            o = sum(per[g][1] for g in present)
            for g in present[1:]:
                del per[g]
            per[present[0] + " (+%d split out)" % (len(present) - 1)] = [b, o]
            del per[present[0]]

    rank = sorted(((100.0 * o / b, b, o, s) for s, (b, o) in per.items() if b))
    print("\nPER OBJECT -- a helper the original inlined cannot hide here."
          "\nRead this before treating a single-symbol gap as missing code:")
    for pct, b, o, s in rank[:8]:
        print("  ours %5.0f%% of the blob   blob %6d  ours %6d  %s"
              % (pct, b, o, s))

    now = {"identical": len(identical), "same_size": len(samesize),
           "compared": len(common)}
    if args.update:
        with open(RATCHET, "w") as f:
            json.dump(now, f, indent=2, sort_keys=True)
            f.write("\n")
        print("\nratchet updated: %s" % now)
        return 0
    if args.ratchet:
        try:
            was = json.load(open(RATCHET))
        except (OSError, ValueError):
            sys.exit("no %s -- run with --update to set the floor" % RATCHET)
        #
        # ONLY `identical` RATCHETS.  `same_size` counts functions whose byte
        # count agrees and whose instructions do not, and **it goes DOWN when
        # one of them becomes identical** -- which is the outcome this tool
        # exists to encourage.  Ratcheting on it meant the check failed on
        # progress: it sat red while `identical` went 350 to 627 and
        # `same_size` fell 71 to 52, because 277 functions had graduated out
        # of the bucket it was guarding.  A gate that is permanently red
        # protects nothing, and one that fails on improvement teaches people
        # to pass `--update` without reading it.
        #
        # `same_size` is reported with its direction and is informational: a
        # fall is ambiguous (graduation, or a size that stopped matching) and
        # the per-symbol scoring in `byteident.py` is what resolves it.
        #
        bad = [k for k in ("identical",) if now[k] < was[k]]
        if bad:
            print("\nRATCHET FAILED -- the reconstruction moved AWAY from the"
                  "\noriginal's code generation, and no differential test can"
                  "\nsee that:")
            for k in bad:
                print("    %-10s was %d, now %d" % (k, was[k], now[k]))
            print("\n  If the change was deliberate, re-bless with --update"
                  "\n  and say in the commit message why fewer functions match.")
            return 1
        if now["same_size"] < was["same_size"]:
            print("\n  note: same_size %d -> %d.  A FALL IS AMBIGUOUS -- a"
                  "\n  function leaves that bucket both by becoming identical"
                  "\n  and by ceasing to match on size.  identical went %d -> %d"
                  "\n  over the same period; score per-symbol with"
                  "\n  `byteident.py --list-exact` to tell the two apart."
                  % (was["same_size"], now["same_size"],
                     was["identical"], now["identical"]))
        gained = [k for k in now if k in was and now[k] > was[k]]
        print("\nratchet OK%s" % ("" if not gained else
              " -- gained: " + ", ".join("%s %d->%d" % (k, was[k], now[k])
                                         for k in sorted(gained))))
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
