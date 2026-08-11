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
quietly wrong.  This tree already has one of those -- `V90Parameters` is
0x504 in one header and 0x558 in the other, and V90ModemCtor.cpp carries a
long comment about which of the two it must not include, because allocating
the smaller and using the larger under-allocates by 84 bytes and passes every
test that does not run under a checking allocator.

IT WAS ALSO A REAL PORTABILITY WALL.  Six enums were spelled `enum X : int;`
in two headers each.  That is legal for an OPAQUE DECLARATION, which is what
C++11 made them -- and illegal for the C++98 DEFINITION they had to become
for the period compiler to accept them at all.  So the duplication was not a
tidiness question; it was the thing standing between this tree and building
under the compiler that built the object.  See docs/method/compilers.md.

THE TWO ENTRIES BELOW ARE DEBTS, NOT EXEMPTIONS.  Each is a class modelled
twice at two different sizes, which is a gap in the reconstruction rather
than a mistake in the headers -- reconciling them means establishing which
size is right, which is reverse-engineering and not refactoring.  They are
listed so the gate can pass today and so a THIRD one cannot appear quietly.
Removing an entry is progress; adding one needs a reason written here.
"""

import argparse
import collections
import glob
import re
import sys

# type name -> why two definitions are tolerated, for now.
KNOWN = {
    "V90Parameters":
        "two incompatible models: the named 0x558 map in V90Parameters.h and "
        "the 0x504 block in V90PreFilter.h (finding 1112).  Five consumers "
        "need the block form, so it cannot simply move into a .cpp.  Closing "
        "this means settling the size, not moving the text.",
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

    if new or stale:
        return 1
    print("one definition: %d types, %d files, %d known duplicates  OK"
          % (len(found), len({f for fs in found.values() for f in fs}),
             len(dups)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
