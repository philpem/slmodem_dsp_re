#!/usr/bin/env python3
"""Move the aliased blob's linkonce sections out of the way of ours.

THE PROBLEM THIS EXISTS FOR, because it is not obvious and it is fatal.

`.gnu.linkonce.t.NAME` is name-based COMDAT: the linker keeps the first section
it sees with a given name and DISCARDS every later one.  The original was built
by GCC 3.4, which puts every instantiated template member in exactly such a
section, and `dsplibs_ref.o` inherits those names -- 126 of them.

Modern GCC does not use that mechanism.  It emits `.text._ZN...` inside an ELF
section group instead, so our object and the blob's aliased copy have sections
with DIFFERENT names and both survive the link.  That is the only reason the
differential harness works at all, and it is an accident of the compiler being
twenty years newer.

Build our side with the period compiler -- which is the point of
tools/toolchain -- and our sections are named `.gnu.linkonce.t._ZN3AgcIfEC1Ev`
too.  Ours come first on the link line, the blob's copy is discarded whole, and
every `ref_` alias inside it becomes an undefined reference.  Measured on
`t_agc`: four aliases present in `dsplibs_ref.o`, all four undefined at link.

The fix is to take the blob's copies out of the comdat namespace entirely.
`.text.ref_NAME` is an ordinary section, never a duplicate of anything, and
always kept.  The `ref_` in the name is what guarantees it cannot collide with
ours whichever compiler built them.

Nothing else reads these section names: `coverage.py` measures `.text` and
`.gnu.linkonce.t.*` on the BLOB, not on this object.

    refrename.py build/dsplibs_ref.o
"""

import re
import subprocess
import sys


def sections(obj):
    out = subprocess.run(["readelf", "-SW", obj], capture_output=True,
                         text=True).stdout
    return sorted(set(re.findall(r"\s(\.gnu\.linkonce\.t\.\S+)", out)))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__.strip().splitlines()[-1].strip())
    obj = sys.argv[1]
    names = sections(obj)
    if not names:
        return 0                        # already renamed, or nothing to do

    args = ["objcopy"]
    for n in names:
        # `.gnu.linkonce.t.` is 16 characters; keep the mangled tail.
        args += ["--rename-section", "%s=.text.ref_%s" % (n, n[16:])]
    args += [obj, obj + ".tmp"]

    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(r.stderr)
        return r.returncode
    subprocess.run(["mv", obj + ".tmp", obj], check=True)

    left = sections(obj)
    if left:
        sys.stderr.write("refrename: %d linkonce sections survived: %s\n"
                         % (len(left), left[:3]))
        return 1
    print("  refrename: %d linkonce sections moved to .text.ref_*" % len(names))
    return 0


if __name__ == "__main__":
    sys.exit(main())
