#!/usr/bin/env python3
"""
Instruction boundaries, decoded twice and compared.

WHY THIS EXISTS

Finding F737: `cfgsplit.py` dropped 131 bytes of every walk of `v34handshak`
for as long as the tool had existed, because objdump wraps its hex column at
seven bytes and the continuation line carries no mnemonic for the regex to
match.  The tool printed the shortfall on every run -- "61410 bytes accounted
of 61541" -- as a REMARK, so nothing failed and nothing was looked at.

`cfgsplit --selftest` now catches that class, by summing instruction sizes
and comparing against the symbol's size from `nm -S`.  That is genuinely two
sources and it is why the defect cannot come back.

BUT IT CANNOT SEE A WRONG BOUNDARY.  The sizes would still sum correctly if
every instruction START were misplaced -- one instruction two bytes long and
the next two bytes short is invisible to a total.  Boundaries are what a
control-flow walk actually runs on: a branch target that is not an
instruction start silently becomes a block that does not exist.

So this decodes the same bytes with a SECOND, INDEPENDENT decoder and
compares the boundary sets.  `gates.md` rule 4: measure the same thing two
ways and compare, and make the DISAGREEMENT the failure rather than either
number on its own.

    tools/boundarycheck.py --func v34handshak
    tools/boundarycheck.py --all            # every T symbol in .text
    tools/boundarycheck.py --selftest       # prove it can fail

CAPSTONE IS NOT INSTALLED SYSTEM-WIDE, deliberately.  `pip install capstone`
wants --break-system-packages on this host, which is not a thing to do to the
OS python for a cross-check.  Use a venv:

    python3 -m venv /tmp/bc && /tmp/bc/bin/pip install capstone
    /tmp/bc/bin/python tools/boundarycheck.py --func v34handshak

With no capstone this exits 77 and says so, rather than passing vacuously --
a check that silently does nothing is the defect this tool exists to catch.
"""

import argparse
import os
import subprocess
import sys

#
# `tools/dis.py` shadows the standard library's `dis`, and `inspect` imports
# it, so anything here that reaches for pyelftools dies with
#
#     AttributeError: module 'dis' has no attribute 'COMPILER_FLAG_NAMES'
#
# naming neither the directory nor the file.  Same fix as whichfield.py.
#
_here = os.path.abspath(os.path.dirname(__file__))
sys.path[:] = [p for p in sys.path if os.path.abspath(p or ".") != _here]

try:
    import capstone
except ImportError:
    capstone = None


def symbols(obj):
    """Every T/t symbol in .text, as name -> (addr, size)."""
    out = subprocess.run(["nm", "-S", "--defined-only", obj],
                         capture_output=True, text=True).stdout
    syms = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) == 4 and f[2] in ("T", "t") and int(f[1], 16) > 0:
            syms[f[3]] = (int(f[0], 16), int(f[1], 16))
    return syms


def text_bytes(obj):
    """`.text` as raw bytes, and the address it starts at."""
    out = subprocess.run(["objdump", "-h", obj], capture_output=True,
                         text=True).stdout
    base = None
    for line in out.splitlines():
        f = line.split()
        if len(f) >= 6 and f[1] == ".text":
            base = int(f[3], 16)
            break
    if base is None:
        sys.exit("no .text in %s" % obj)
    raw = subprocess.run(["objcopy", "-O", "binary", "--only-section=.text",
                          obj, "/dev/stdout"], capture_output=True).stdout
    return base, raw


def objdump_bounds(obj, lo, hi):
    """Instruction start addresses according to objdump."""
    out = subprocess.run(["objdump", "-d", "-j", ".text",
                          "--start-address=0x%x" % lo,
                          "--stop-address=0x%x" % hi, obj],
                         capture_output=True, text=True).stdout
    starts = set()
    for line in out.splitlines():
        line = line.replace("\t", " ")
        head = line.split(":", 1)
        if len(head) != 2 or not head[0].strip():
            continue
        try:
            addr = int(head[0].strip(), 16)
        except ValueError:
            continue
        # A continuation line is address + bytes and nothing else; it is not
        # a boundary.  Distinguish it by there being no mnemonic after the
        # hex, which is exactly what finding F737's regex got wrong -- but
        # here getting it wrong is SAFE, because a spurious extra boundary
        # shows up as a disagreement rather than as silence.
        rest = head[1].split()
        n = 0
        while n < len(rest) and len(rest[n]) == 2 and \
                all(c in "0123456789abcdef" for c in rest[n]):
            n += 1
        if n < len(rest):
            starts.add(addr)
    return starts


def capstone_bounds(base, raw, lo, hi):
    """Instruction start addresses according to capstone, decoded linearly."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = False
    off = lo - base
    starts = set()
    for insn in md.disasm(raw[off:hi - base], lo):
        starts.add(insn.address)
    return starts


def check(obj, name, lo, size, base, raw, verbose):
    hi = lo + size
    a = objdump_bounds(obj, lo, hi)
    b = capstone_bounds(base, raw, lo, hi)
    only_a, only_b = a - b, b - a
    if not only_a and not only_b:
        return True, len(a)
    if verbose:
        for addr in sorted(only_a)[:5]:
            print("      objdump only  0x%x" % addr)
        for addr in sorted(only_b)[:5]:
            print("      capstone only 0x%x" % addr)
    return False, len(a)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--obj", default="ref/slmodemd/dsplibs.o")
    ap.add_argument("--func")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    if capstone is None:
        print("boundarycheck: capstone is not importable, so NOTHING WAS\n"
              "CHECKED.  See this file's header for the venv recipe.  Exiting\n"
              "77 rather than 0: a check that silently does nothing is the\n"
              "defect this tool exists to catch.")
        return 77

    syms = symbols(args.obj)
    base, raw = text_bytes(args.obj)

    if args.selftest:
        #
        # THE NEGATIVE CONTROL.  Decode one function at a deliberately wrong
        # offset and require the comparison to notice.  Without this a clean
        # run proves only that the tool ran -- finding F134, and `extcheck`
        # printing "(none)" through four broken versions.
        #
        lo, size = syms["v34handshak"]
        good = capstone_bounds(base, raw, lo, lo + size)
        skew = capstone_bounds(base, raw, lo + 1, lo + size)
        print("  ok    boundaries agree with themselves   %6d" % len(good))
        if good == skew:
            print("\n  ERROR: decoding from a different offset produced the\n"
                  "  SAME boundary set, so this comparison cannot fail and a\n"
                  "  pass means nothing.")
            return 1
        print("  ok    a one-byte skew is detected        %6d differ"
              % len(good ^ skew))
        okc, _ = check(args.obj, "v34handshak", lo, size, base, raw, False)
        print("  %-4s  objdump vs capstone, v34handshak"
              % ("ok" if okc else "FAIL"))
        return 0 if okc else 1

    if args.all:
        bad = 0
        for name in sorted(syms):
            lo, size = syms[name]
            ok, n = check(args.obj, name, lo, size, base, raw, args.verbose)
            if not ok:
                bad += 1
                print("  DISAGREE  %-40s %6d boundaries" % (name, n))
        print("\n  %d symbols checked, %d disagree" % (len(syms), bad))
        return 1 if bad else 0

    if not args.func:
        ap.error("--func is required (or --all, or --selftest)")
    if args.func not in syms:
        sys.exit("%s is not a sized T/t symbol in %s" % (args.func, args.obj))
    lo, size = syms[args.func]
    ok, n = check(args.obj, args.func, lo, size, base, raw, True)
    print("  %-4s  %-30s %6d boundaries, %d bytes"
          % ("ok" if ok else "FAIL", args.func, n, size))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
