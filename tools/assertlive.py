#!/usr/bin/env python3
"""Are the guarded offset assertions ALIVE in the period configuration?

WHY THIS EXISTS

The tree asserts its structure layout at compile time:

    typedef char v8_off_rx[(__builtin_offsetof(struct v8, rx) == 0x24) ? 1 : -1];

and guards each one on `__SIZEOF_POINTER__ == 4`, because the reconstruction
targets a 32-bit object and **the layout is expected to move when the tree is
built 64-bit**.  That is deliberate and correct: the period compiler decides,
the 64-bit build is a portability check, and an offset that shifts with pointer
width is not a defect.

THE FAILURE MODE, WHICH HAS HAPPENED

`__SIZEOF_POINTER__` is a GCC 4.6+ predefine.  GCC 3.4.2 does not have it, so
`#if __SIZEOF_POINTER__ == 4` reads `#if 0` and **every assertion in the file
vanishes while the file compiles clean** -- structural checks deleted, exit 0,
nothing printed.  `period.mk` and `period_inner.sh` both pass
`-D__SIZEOF_POINTER__=4` to prevent it, and the day either stops doing so the
tree loses its layout checks in silence.

Measured, that silence is total:

    __SIZEOF_POINTER__ == 4      1747 assertions live over 93 files
    __SIZEOF_POINTER__ == 8        42 live over 6 files
    UNDEFINED                      42 live over 6 files   <-- identical to 64-bit

The undefined case is byte-for-byte indistinguishable from the 64-bit one, so
no build output can tell them apart.  The 42 survivors are the pointer-free
structures whose layout genuinely does not move -- `v8_off_rx`, the two `Jd`
unpackers, three size checks -- which is why the count does not fall to zero
and why "zero assertions" is the wrong thing to test for.

WHAT THIS CHECKS -- THREE THINGS, BECAUSE THEY FAIL DIFFERENTLY

1. **Defining `__SIZEOF_POINTER__=4` must INCREASE the live count.**  If it
   does not, every guard is inert.  Self-validating: no recorded number to go
   stale, and it cannot pass because somebody forgot to update a baseline.
2. **Both period build scripts must still pass the flag.**  A comparison run
   with THIS tool's flags cannot see a missing `-D` in `period.mk` -- and a
   missing `-D` in `period.mk` is exactly the failure that happened.  Only
   reading the scripts catches it.
3. **The count must not DECREASE.**  Checks 1 and 2 both pass while a single
   header's guard goes inert: injecting `#if 0` over `V90Parameters.h`'s guard
   took the tree from 1747 assertions to 1718 and this tool still said OK.
   Printing a number is not the same as failing on it.  So there is a floor in
   `tools/assertlive.json`, in the same shape as `compare.py --ratchet`: it
   fails on a decrease only, and rises when the tree gains assertions.

Findings F134, F2400, F2401 and F3110 are the same shape -- a detector reporting on
nothing and rendering as a pass -- so this one prints its denominator on every
line that carries a verdict.
"""

import concurrent.futures
import glob
import json
import os
import re
import subprocess
import sys

#
# The assertion idiom, in every spelling the tree uses.  `_size` catches the
# whole-structure checks (`cid_size_check`), `_off` the per-field ones.
#
PAT = re.compile(r"typedef\s+char\s+[A-Za-z0-9_]*(?:_off|_size)[A-Za-z0-9_]*\s*\[")


def sources():
    out = []
    for pat in ("src/**/*.c", "src/**/*.cpp"):
        out += glob.glob(pat, recursive=True)
    return sorted(out)


def live(path, ptr):
    """Assertions surviving preprocessing of `path` at the given pointer size.

    `ptr` of None means UNDEFINED, which is the failure mode being tested for.
    """
    cxx = path.endswith(".cpp")
    cmd = ["g++", "-m32", "-nostdinc++"] if cxx else ["gcc", "-m32"]
    cmd += ["-E", "-Iinclude", "-Ithird_party/slmodem", "-U__SIZEOF_POINTER__"]
    if ptr is not None:
        cmd.append("-D__SIZEOF_POINTER__=%d" % ptr)
    r = subprocess.run(cmd + [path], capture_output=True, text=True)
    #
    # A file that fails to preprocess is not evidence of anything and must not
    # be silently counted as zero -- that is the dead detector again.
    #
    if r.returncode:
        return None
    return len(PAT.findall(r.stdout))


def sweep(paths, ptr):
    total = files = broken = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as ex:
        for n in ex.map(lambda p: live(p, ptr), paths):
            if n is None:
                broken += 1
                continue
            if n:
                files += 1
            total += n
    return total, files, broken


#
# THE HISTORICAL FAILURE WAS NOT A BROKEN GUARD, IT WAS A MISSING FLAG, and a
# comparison run with this tool's OWN flags cannot see that.  Preprocessing
# proves the guards are CAPABLE of being live; only reading the build scripts
# proves the period build actually makes them so.  Both are needed and they
# fail differently: a moved `#endif` breaks the first, a dropped `-D` the
# second.
#
BUILDERS = ("tools/toolchain/period.mk", "tools/toolchain/period_inner.sh")


def flag_present():
    """Which period build scripts still pass -D__SIZEOF_POINTER__=4."""
    out = []
    for f in BUILDERS:
        if not os.path.exists(f):
            out.append((f, None))
            continue
        out.append((f, "-D__SIZEOF_POINTER__=4" in open(f).read()))
    return out


FLOOR = "tools/assertlive.json"


def load_floor():
    if not os.path.exists(FLOOR):
        return None
    return json.load(open(FLOOR)).get("live_at_32")


def main():
    paths = sources()
    if not paths:
        sys.exit("assertlive.py: no sources under src/ -- every count below "
                 "would be computed against NOTHING and render as a clean "
                 "zero.  Findings F2400, F2401.")

    on, on_files, broken = sweep(paths, 4)
    off, off_files, _ = sweep(paths, None)

    print("offset assertions, over %d source file(s):" % len(paths))
    print("  __SIZEOF_POINTER__=4 (the period build) : %5d live over %d file(s)"
          % (on, on_files))
    print("  __SIZEOF_POINTER__ undefined            : %5d live over %d file(s)"
          % (off, off_files))

    if broken:
        print("  %d file(s) failed to preprocess and were NOT counted" % broken)

    if on <= off:
        sys.exit(
            "\nassertlive.py: DEFINING __SIZEOF_POINTER__=4 GAINS NOTHING "
            "(%d vs %d).\n"
            "  The guarded layout assertions are inert, so the period build is\n"
            "  compiling with its structural checks deleted and exiting 0.\n"
            "  Check that -D__SIZEOF_POINTER__=4 is still in BOTH\n"
            "  tools/toolchain/period.mk and tools/toolchain/period_inner.sh,\n"
            "  and that no `#endif` has been moved so as to swallow a guarded\n"
            "  region (finding F7799)." % (on, off))

    floor = load_floor()
    if floor is not None and on < floor:
        sys.exit(
            "\nassertlive.py: THE LIVE ASSERTION COUNT FELL, %d -> %d.\n"
            "  Checks 1 and 2 above both pass while one header's guard goes\n"
            "  inert, so a drop is the only signal that remains.  Either a\n"
            "  guard stopped covering its assertions -- a moved `#endif`,\n"
            "  finding F7799 -- or assertions were deleted.  If the removal was\n"
            "  deliberate, lower the floor in tools/assertlive.json in the same\n"
            "  commit and say why." % (floor, on))

    bad = [f for f, ok in flag_present() if not ok]
    if bad:
        sys.exit(
            "\nassertlive.py: %s DOES NOT PASS -D__SIZEOF_POINTER__=4.\n"
            "  The guards below are live and capable, but the period compiler\n"
            "  has no such predefine -- GCC 3.4.2 predates it -- so every one\n"
            "  of the %d guarded assertions reads `#if 0` in that build and\n"
            "  VANISHES while the file compiles clean.  This is the failure\n"
            "  that already happened once; see CLAUDE.md."
            % (", ".join(bad), on - off))

    print("  guard is live: %d assertion(s) exist only at 32 bits  OK" % (on - off))
    print("  -D__SIZEOF_POINTER__=4 present in: %s  OK"
          % ", ".join(os.path.basename(f) for f, _ in flag_present()))
    print("  the %d that survive undefined are the pointer-free structures "
          "whose\n  layout does not move with pointer width, which is why "
          "'zero' is the\n  wrong thing to test for." % off)
    return 0


if __name__ == "__main__":
    sys.exit(main())
