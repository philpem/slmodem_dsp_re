#!/usr/bin/env python3
"""
Check every `/* +0xNNN */` in the headers against what the compiler lays out.

WHY THIS EXISTS

The struct definitions are the reconstruction's central claim: this field is
that offset in the object, and every differential test that pokes a field
depends on it.  They are held together by `unmapped_XXXX[0xB - 0xA]` padding
arrays, and a padding array is stated as an ABSOLUTE SPAN -- so getting one
wrong slides every field after it and the error propagates to the end of the
struct.  There are hand-written `V34OB_ASSERT`-style checks in a few .c files,
but they cover a few dozen fields out of nearly nine hundred, and they were
written next to the code that needed them rather than to cover the struct.

The gap showed up in a merge.  Two branches grew `struct v34_object` from the
same base and the paddings between their fields had to be recomputed by hand
to interleave them; nothing in the tree could have told the difference between
getting that right and getting it wrong by two bytes, because both compile and
both pass every test that does not happen to poke a field past the mistake.

Each annotation is already the claim.  This just holds the compiler to it.

    tools/offcheck.py            check, print a count, fail on a mismatch
    tools/offcheck.py --emit     print the generated C instead

ONE CONVENTION IS NOT THIS ONE.  `struct v8_v21_params` annotates each field
with where the STRUCT sits in the V.8 object -- `short f00; /* +0xc20 */` --
rather than with the field's own offset.  Its twelve are skipped by name; a
new struct that does the same will show up as twelve sudden failures, and the
answer is to add it here rather than to renumber it.
"""

import glob
import re
import subprocess
import sys

# Structs whose annotations are object-relative rather than field-relative.
OBJECT_RELATIVE = {"v8_v21_params"}

#
# C++, so they cannot go into the C translation unit this builds -- and
# `offsetof` could not reach their members anyway, which are private.
#
# Their layouts are not unchecked, they are checked harder: each has a
# differential test that reads our object through a plain struct of the
# intended shape and compares it field by field against the blob's own
# object after every operation.  A wrong offset shows up as a failure on
# real data rather than as a compile-time assertion about our own header.
#
SKIP_HEADERS = ("Agc.h", "DiffCoder.h", "DspMath.h", "FloatFIR.h", "FloatIIR.h", "GenericIIR.h", "LowPassFIR.h", "Queue.h", "Scrambler.h", "SineWave.h", "V90Jd.h", "V90Phase3Modulator.h", "V90PreFilter.h", "V92Jd.h", "x87copy.h")


def headers():
    return [h for h in sorted(glob.glob("include/dsplib/*.h"))
            if not h.endswith(SKIP_HEADERS)]


def annotations():
    """(struct, field, offset) for every annotated field, outermost only."""
    out = []
    for h in headers():
        for m in re.finditer(r"(?m)^struct\s+(\w+)\s*\{(.*?)^\};",
                             open(h).read(), re.S):
            name, body = m.group(1), m.group(2)
            if name in OBJECT_RELATIVE:
                continue
            depth = 0
            for line in body.split("\n"):
                # A nested struct or union's members belong to the nested
                # type; only depth 0 is a member of this one.
                if depth == 0:
                    f = re.match(r"\s+(?:const\s+)?[A-Za-z_][\w\s\*]*?(\w+)"
                                 r"(?:\[[^\]]*\])?\s*;\s*/\*\s*"
                                 r"\+0x([0-9a-fA-F]+)", line)
                    if f and not f.group(1).startswith(("unmapped", "pad",
                                                        "gap")):
                        out.append((name, f.group(1), int(f.group(2), 16)))
                depth += line.count("{") - line.count("}")
    return out


def emit(anns):
    src = ["#include <stddef.h>"]
    src += ['#include "%s"' % h[len("include/"):] for h in headers()]
    src.append("#define OFF(s, f, o, tag) typedef char offcheck_##tag[ \\\n"
               "    ((int)__builtin_offsetof(struct s, f) == (o)) ? 1 : -1];")
    for i, (s, f, o) in enumerate(anns):
        src.append("OFF(%s, %s, 0x%x, %d)" % (s, f, o, i))
    return "\n".join(src) + "\n"


def main():
    anns = annotations()
    src = emit(anns)
    if "--emit" in sys.argv:
        sys.stdout.write(src)
        return 0

    r = subprocess.run(["gcc", "-m32", "-Iinclude", "-fsyntax-only",
                        "-xc", "-"], input=src, capture_output=True, text=True)
    if r.returncode == 0:
        print("offsets: %d annotations, all match __builtin_offsetof  OK"
              % len(anns))
        return 0

    bad = sorted(set(int(t) for t in re.findall(r"offcheck_(\d+)", r.stderr)))
    for i in bad:
        s, f, o = anns[i]
        print("  MISMATCH  struct %s.%s says +0x%x" % (s, f, o))
    if not bad:                     # something else broke; show it whole
        sys.stderr.write(r.stderr)
    print("\n  %d of %d annotations do not match the layout"
          % (len(bad) or len(anns), len(anns)))
    return 1


if __name__ == "__main__":
    sys.exit(main())
