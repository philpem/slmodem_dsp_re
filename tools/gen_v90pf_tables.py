#!/usr/bin/env python3
"""Emit V90PreFilter's static data members as C++.

The class has ten of them, 23,860 bytes, and every one is a defined `D`
symbol in the blob -- therefore renamed `ref_*` in the reference object,
therefore not satisfiable from it.  `dataBase` is what forces the whole set:
it is read by three of batch 3's five methods, and its own 16 relocations
point at `refLoopsType1,2,4,5,6,7`, which nothing in .text reaches from this
batch at all.  A scan of `.rel.text` says those six belong to #59; a scan of
`.rel.data` says they cannot be left out.  See finding 234.

Floats are emitted as %.17g and the round trip is checked against the original
four bytes before anything is written, so the output is bit-exact by
construction and not by inspection.  Values that do not round-trip are emitted
as C hex float literals instead; nothing in these tables needs that today.

Usage:  python3 tools/gen_v90pf_tables.py ../slmodemd/dsplibs.o
"""

import re
import struct
import subprocess
import sys

# .data offset, element count.  Sizes are st_size from `nm -S`; the record
# shapes are read out of the code that indexes them (autoSelection walks
# `dataBase` 36 bytes at a time and each loop array 0x44).
COEF = [
    ("preFilterCoefType1", 0x0C00, 31, 20),
    ("preFilterCoefType2", 0x15C0, 31, 20),
    ("preFilterCoefType3", 0x1F80, 31, 40),
]

# name, .data offset, records IN THE BLOB.  refLoopsType2 is the odd one --
# see the comment where it is emitted.
LOOPS = [
    ("refLoopsType1", 0x6140, 23),
    ("refLoopsType2", 0x5860, 33),
    ("refLoopsType4", 0x4EC0, 36),
    ("refLoopsType5", 0x4520, 36),
    ("refLoopsType6", 0x3C00, 34),
    ("refLoopsType7", 0x32E0, 34),
]

DATABASE = (0x6760, 17)


def sections(obj):
    out = subprocess.run(["readelf", "-S", "-W", obj],
                         capture_output=True, text=True).stdout
    sec = {}
    for line in out.splitlines():
        m = re.match(r"\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+"
                     r"([0-9a-f]+)\s+([0-9a-f]+)", line)
        if m:
            sec[m.group(1)] = (int(m.group(4), 16), int(m.group(5), 16))
    return sec


def relocs(obj):
    """dataBase's outgoing pointers, as {offset: symbol}."""
    out = subprocess.run(["readelf", "-r", "-W", obj],
                         capture_output=True, text=True).stdout
    cur, hits = None, {}
    for line in out.splitlines():
        m = re.match(r"Relocation section '(\S+)'", line)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r"([0-9a-f]{8})\s+\S+\s+\S+\s+\S+\s+(\S+)", line)
        if m and cur == ".rel.data":
            hits[int(m.group(1), 16)] = m.group(2)
    return hits


def fl(raw):
    """One float, as a literal that reproduces these four bytes exactly."""
    v = struct.unpack("<f", raw)[0]
    for fmt in ("%.9g", "%.17g"):
        s = fmt % v
        if struct.pack("<f", float(s)) == raw:
            if "." not in s and "e" not in s and "n" not in s:
                s += ".0"
            return s + "f"
    return float.hex(v) + "f"


def main():
    obj = sys.argv[1]
    sec = sections(obj)
    off = sec[".data"][0]
    blob = open(obj, "rb").read()
    rel = relocs(obj)

    def D(o, n):
        return blob[off + o:off + o + n]

    coefs = []
    for name, base, rows, cols in COEF:
        body = []
        for r in range(rows):
            vals = [fl(D(base + (r * cols + c) * 4, 4)) for c in range(cols)]
            lines = []
            for i in range(0, cols, 4):
                lines.append("\t\t" + ", ".join(vals[i:i + 4]) + ",")
            body.append("\t{\t/* %d */\n%s\n\t}," % (r, "\n".join(lines)))
        coefs.append((name, rows, cols, "\n".join(body)))

    loops = {}
    for name, base, count in LOOPS:
        body = []
        for k in range(count):
            rec = D(base + 68 * k, 68)
            nm = rec[:32]
            z = nm.index(b"\0")
            assert all(b == 0 for b in nm[z:]), name
            sig = ", ".join(fl(rec[32 + 4 * i:36 + 4 * i]) for i in range(6))
            a, b, c = struct.unpack("<3i", rec[56:68])
            body.append('\t{ "%s",\n\t  { %s },\n\t  %d, %d, %d },'
                        % (nm[:z].decode("ascii"), sig, a, b, c))
        loops[name] = (count, "\n".join(body))

    base, count = DATABASE
    db = []
    for k in range(count):
        rec = D(base + 36 * k, 36)
        z = rec[:32].index(b"\0")
        assert all(x == 0 for x in rec[z:32])
        tgt = rel.get(base + 36 * k + 0x20)
        if tgt is None:
            assert struct.unpack("<I", rec[32:])[0] == 0
            ptr = "0"
        else:
            ptr = "V90PreFilter::" + re.match(
                r"_ZN12V90PreFilter\d+(\w+?)E$", tgt).group(1)
        db.append('\t{ "%s", %s },' % (rec[:z].decode("ascii"), ptr))

    open("src/pump/v90/V90PreFilter_coeffs.cpp", "w").write(COEF_TMPL % {
        "bodies": "\n\n".join(
            "float V90PreFilter::%s[%d][%d] = {\n%s\n};" % (n, r, c, b)
            for n, r, c, b in coefs)})

    open("src/pump/v90/V90PreFilter_loops.cpp", "w").write(LOOP_TMPL % {
        "loops": "\n\n".join(
            "V90RefLoop V90PreFilter::%s[%d] = {\n%s%s\n};"
            % (n, loops[n][0] + (1 if n == "refLoopsType2" else 0),
               loops[n][1],
               TYPE2_TERM if n == "refLoopsType2" else "")
            for n, _, _ in LOOPS),
        "database": "\n".join(db)})


TYPE2_TERM = """
\t/*
\t * NOT IN THE BLOB'S SYMBOL.  refLoopsType2 is 2244 bytes, exactly 33
\t * records, and none of them has a zero first byte -- so the terminator
\t * the counting loops look for is the .data alignment padding that
\t * happens to follow the symbol.  A 34th, all-zero record is added here
\t * because our copy has no such padding to rely on; the counted length
\t * is 33 either way, which is what the differential test asserts.
\t */
\t{ "", { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }, 0, 0, 0 },"""

COEF_TMPL = """\
/*
 * V90PreFilter_coeffs.cpp -- the three pre-filter coefficient banks.
 *
 * GENERATED by tools/gen_v90pf_tables.py from dsplibs.o .data -- do not edit.
 *
 * These are the original bytes, kept verbatim so the reconstruction is
 * bit-identical to the blob.  test/unit/t_v90pftab.cpp compares every one of
 * them against the blob's own copy through its ref_ alias, so the claim is
 * checked rather than asserted.
 *
 * The shapes come out of the address arithmetic in `getFilterPointer`,
 * `setFilter` and `selectFilter`, which is the only place they are written
 * down.  Types 1 and 2 are indexed `base + 80 * n` with a length of 20 floats
 * and `n` clamped to 30, so 31 rows of 20.  Type 3 is `base - 3200 + 160 * n`
 * with a length of 40 and `n` clamped to 50, so 31 rows of 40 with row zero
 * at n = 20 -- the negative displacement is in the object, not a mistake
 * here, and the clamp is what keeps it in range.
 *
 * NOT const: the blob's symbols are `D`, and `FloatFIR::setCoefficients`
 * takes a plain `float *`.
 */

#include "dsplib/V90PreFilter.h"

%(bodies)s
"""

LOOP_TMPL = """\
/*
 * V90PreFilter_loops.cpp -- the codec table and the six reference-loop sets.
 *
 * GENERATED by tools/gen_v90pf_tables.py from dsplibs.o .data -- do not edit.
 *
 * WHY THE refLoopsType* TABLES ARE HERE.  A scan of the blob's .rel.text says
 * they are referenced by exactly one function, `VPcmV34InitiateRetrain`,
 * which belongs to task #59 and not to this batch.  That scan is looking in
 * the wrong section: `dataBase` is read by three of batch 3's five methods,
 * and `dataBase`'s OWN sixteen relocations point at these six.  Leaving them
 * out fails the link for the whole test suite.  Finding 234.
 *
 * The 68-byte record is out of the code that indexes it: `autoSelection`
 * walks it 0x44 at a time and compares its first byte against zero to find
 * the end, sums the squared differences of six floats at +0x20, and returns
 * the int at +0x3c; `selectFilter` and `getFilterPointer` switch on the int
 * at +0x38; `isV90WithEia6` and `getV90Capability` test the int at +0x40.
 * The 36-byte `dataBase` record is a 32-byte name -- `edprintf` prints it
 * with %%s -- and the pointer at +0x20.
 *
 * NOT const: the blob's symbols are `D`.
 */

#include "dsplib/V90PreFilter.h"

%(loops)s

/*
 * The codec table.  Seventeen entries, the last of them empty: the
 * constructor counts them by walking until the name's first byte is zero and
 * clamps its argument to count - 1, so entry 16 is the terminator and
 * `__tHardwareCodecTypes__` has sixteen values.
 */
V90CodecEntry V90PreFilter::dataBase[17] = {
%(database)s
};
"""


if __name__ == "__main__":
    main()
