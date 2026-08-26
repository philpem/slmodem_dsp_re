#!/usr/bin/env python3
"""onedef.py -- one type, one home.  Fail on a NEW second definition.

    tools/onedef.py            # gate; part of `make phase`
    tools/onedef.py --list     # every type and where it is defined

THE RULE.  A `class`, `struct`, `enum` or `union` is DEFINED in exactly one
file.  Everyone else forward-declares it or includes that file.  A forward
declaration is not a definition and is never a problem.

WHY IT IS A GATE AND NOT A STYLE NOTE.  Two definitions of one type is
undefined behaviour the moment both reach a translation unit, and the failure
is silent until it is catastrophic: the compiler picks one, and every offset,
every `sizeof` and every allocation in the other half of the program is
quietly wrong.  This tree HAD one of those.  `V90Parameters` was 0x504 in one
header and 0x558 in the other, so a translation unit holding the smaller and
allocating from `sizeof` under-allocated by 84 bytes and passed every test not
run under a checking allocator.  It also broke `tools/whichfield.py`, which
resolved every offset of that class to `b[8] (unsigned char)` -- the tool
CLAUDE.md points you at to turn a differential offset into a diagnosis, giving
no diagnosis.  Reconciled by task #116; the gate reported it STALE the moment
it was, which is how a resolved debt is supposed to surface.

IT WAS ALSO A REAL PORTABILITY WALL.  Six enums were spelled `enum X : int;`
in two headers each.  That is legal for an OPAQUE DECLARATION, which is what
C++11 made them -- and illegal for the C++98 DEFINITION they had to become
for the period compiler to accept them at all.  So the duplication was not a
tidiness question; it was the thing standing between this tree and building
under the compiler that built the object.  See docs/method/compilers.md.

THE ENTRY BELOW IS A DEBT, NOT AN EXEMPTION.  It is a class modelled twice at
two different sizes, which is a gap in the reconstruction rather than a
mistake in the headers -- reconciling it means establishing which size is
right, which is reverse-engineering and not refactoring.  It is listed so the
gate can pass today and so a SECOND one cannot appear quietly.  Removing an
entry is progress; adding one needs a reason written here.
"""

import argparse
import collections
import glob
import re
import sys

# type name -> why two definitions are tolerated, for now.
KNOWN = {
    "V90Phase4Demodulator":
        "a partial model in V90SessionFlag.h -- sessionFlag plus an embedded "
        "V90Phase4Modulator, bounded at 0x2ffc and explicitly 'a floor, not a "
        "size' -- beside the fuller one in V90Phase4Demodulator.h.",
}

DEFN = re.compile(r'^(class|struct|enum|union)\s+(\w+)\s*(?::[^{;]*)?\{', re.M)


def homes():
    """type -> set of files defining it.  Comments stripped first: this file's
    own docstring would otherwise report itself, and several headers quote a
    `struct x {` inside a derivation."""
    found = collections.defaultdict(set)
    for f in sorted(glob.glob('include/**/*.h', recursive=True) +
                    glob.glob('src/**/*.[ch]', recursive=True) +
                    glob.glob('src/**/*.cpp', recursive=True) +
                    glob.glob('src/**/*.hpp', recursive=True)):
        try:
            s = open(f, encoding='latin-1').read()
        except OSError:
            continue
        s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
        s = re.sub(r'//[^\n]*', '', s)
        for _, name in DEFN.findall(s):
            found[name].add(f)
    return found


#
# ONE TYPE ONE HOME GATES A TYPE; A MACRO IS NOT A TYPE, AND THAT WAS THE HOLE.
# Two parallel headers gave `V32_OBJ_STATUS` two different offsets, 0x30 and
# 0x31.  Each header was internally consistent, each suite was green, and the
# pair is legal C right up until one translation unit includes both -- at which
# point one of them silently wins and every field after it is wrong in the
# other half.  No differential test can see that, because until the collision
# happens both halves are self-consistent.  Finding F8206.
#
# Only OBJECT-LIKE macros are compared.  A function-like macro's body is not a
# value and two spellings of one can be equivalent; an offset is a number and
# two numbers either agree or they do not.
#
MACRO_DEF = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)\s+(\S.*?)\s*(?:/\*.*)?$")


def macro_clashes():
    """(clashing macros, count of benign multiply-defined ones)."""
    val = {}
    for pat in ("include/**/*.h", "src/**/*.h"):
        for f in sorted(glob.glob(pat, recursive=True)):
            for line in open(f, errors="surrogateescape"):
                m = MACRO_DEF.match(line)
                if m and "(" not in m.group(1):
                    val.setdefault(m.group(1), {})[f] = m.group(2).strip()
    multi = {k: v for k, v in val.items() if len(v) > 1}
    clash = {k: v for k, v in multi.items() if len(set(v.values())) > 1}
    return clash, len(multi) - len(clash)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    found = homes()
    if args.list:
        for name, fs in sorted(found.items()):
            print("%-34s %s" % (name, "  ".join(sorted(fs))))
        return 0

    dups = {n: fs for n, fs in found.items() if len(fs) > 1}
    new = {n: fs for n, fs in dups.items() if n not in KNOWN}
    stale = [n for n in KNOWN if n not in dups]

    for name, fs in sorted(new.items()):
        print("  DUPLICATE  %s defined in %d files:" % (name, len(fs)))
        for f in sorted(fs):
            print("               %s" % f)
        print("             One type, one home.  Forward-declare it, include "
              "its header, or")
        print("             register it in tools/onedef.py with the reason.")

    if stale:
        # A known duplicate that resolved itself is good news the gate should
        # not swallow: leaving it listed lets the next one in unnoticed.
        print("  STALE      no longer duplicated, drop from KNOWN: %s"
              % ", ".join(sorted(stale)))

    clash, seen = macro_clashes()
    for name, where in sorted(clash.items()):
        print("  MACRO      %s has DIFFERENT values in %d headers:" % (name, len(where)))
        for f, v in sorted(where.items()):
            print("               %-50s %s" % (f, v[:44]))
        print("             Each header is internally consistent and both compile;")
        print("             this is legal C until one TU includes both, and no")
        print("             differential test can see it.  Finding F8206.")

    if new or stale or clash:
        return 1
    print("one definition: %d types, %d files, %d known duplicates  OK"
          % (len(found), len({f for fs in found.values() for f in fs}),
             len(dups)))
    print("one value:      %d object-like macro(s) defined in more than one "
          "header, all agreeing  OK" % seen)
    return 0


if __name__ == "__main__":
    sys.exit(main())
