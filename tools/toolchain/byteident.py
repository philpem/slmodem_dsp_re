#!/usr/bin/env python3
"""Per-function POSITIONAL BYTE IDENTITY: did we reproduce the function exactly?

WHY THIS EXISTS, AND WHY `compare.py` IS NOT IT

`compare.py` runs `objdump -d --no-show-raw-insn` and compares MNEMONICS with
operands dropped.  That is deliberate and it answers a real question, but it
cannot answer the first one anybody actually asks: **is our function the same
bytes in the same places as the object's?**  Two functions storing different
constants to different offsets both read as `mov mov mov` to that tool.

Its other number is worse for this purpose.  The "total code: blob N bytes,
ours M (P%)" line is a SIZE RATIO over the whole tree, and a size ratio moves
when we emit more code, not when we emit more of the right code -- `-O3` took
it from 77.3% to 89.0% while the count of matching functions did not move at
all (finding 616).  It is a rough gauge of completion and nothing else, and it
has already misled once: the MP CRC work found a source form whose BYTE COUNT
was closer while its instruction sequence was further away.

So this tool reports one number per function and one count over the tree:
**exact, or not.**

RELOCATED FIELDS ARE COMPARED BY TARGET, NOT BY VALUE, and that is not a
loosening -- it is the only correct comparison.  A four-byte field under an
`R_386_32` or `R_386_PC32` holds a placeholder the linker will overwrite; in
the blob it is frequently 0 or -4, and ours is whatever our assembler chose.
Comparing those bytes literally would report a difference that does not exist,
so each relocated field is compared by (relocation type, target symbol)
instead.  A function that is byte-equal everywhere else but CALLS SOMETHING
DIFFERENT is reported separately, as `RELOC`, because that is a real
difference this tool must not hide.

    tools/toolchain/byteident.py                  counts, then the misses
    tools/toolchain/byteident.py --list-exact     the exact set, for a diff
    BLOB=/abs/path/to/dsplibs.o TC_OUT=build/tc_out tools/toolchain/byteident.py

Denominator: the symbols the blob and `TC_OUT` both define, which is
`compare.py`'s denominator exactly, so the two numbers are comparable.
"""

import argparse
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))


def _default_blob():
    d = subprocess.run(["git", "rev-parse", "--git-common-dir"],
                       capture_output=True, text=True).stdout.strip()
    if d:
        return os.path.join(os.path.dirname(os.path.abspath(d)),
                            "ref", "slmodemd", "dsplibs.o")
    return os.path.join("ref", "slmodemd", "dsplibs.o")


BLOB = os.environ.get("BLOB") or _default_blob()
OURS = os.environ.get("TC_OUT", "build/tc_out")

INSN = re.compile(r"^\s*([0-9a-f]+):\t((?:[0-9a-f]{2} )+)")
RELOC = re.compile(r"^\s*([0-9a-f]+):\s+(R_386_\w+)\s+(\S+)")


def sizes(path):
    out = subprocess.run(["nm", "--size-sort", "-S", path],
                         capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "TtWw":
            d[p[3]] = int(p[1], 16)
    return d


def body(path, sym):
    """(bytes, {offset: (type, target)}) for one function, offsets relative."""
    out = subprocess.run(
        ["objdump", "-dr", "--disassemble=" + sym, path],
        capture_output=True, text=True).stdout
    data, relocs, base = {}, {}, None
    for line in out.splitlines():
        m = INSN.match(line)
        if m:
            at = int(m.group(1), 16)
            if base is None:
                base = at
            for i, byte in enumerate(m.group(2).split()):
                data[at - base + i] = int(byte, 16)
            continue
        m = RELOC.match(line)
        if m and base is not None:
            relocs[int(m.group(1), 16) - base] = (m.group(2), m.group(3))
    if base is None:
        return None, None
    n = max(data) + 1 if data else 0
    return bytes(data.get(i, 0) for i in range(n)), relocs


REG32 = {"al": "eax", "ah": "eax", "ax": "eax", "eax": "eax",
         "bl": "ebx", "bh": "ebx", "bx": "ebx", "ebx": "ebx",
         "cl": "ecx", "ch": "ecx", "cx": "ecx", "ecx": "ecx",
         "dl": "edx", "dh": "edx", "dx": "edx", "edx": "edx",
         "si": "esi", "esi": "esi", "di": "edi", "edi": "edi",
         "bp": "ebp", "ebp": "ebp", "sp": "esp", "esp": "esp"}
REGTOK = re.compile(r"%(\w+)")


def insns(path, sym):
    """[(mnemonic, operand text)] with addresses and symbol comments dropped."""
    out = subprocess.run(
        ["objdump", "-dr", "--disassemble=" + sym, "--no-show-raw-insn", path],
        capture_output=True, text=True).stdout
    rows, base = [], None
    for line in out.splitlines():
        m = re.match(r"^\s*([0-9a-f]+):\t", line)
        if not m:
            #
            # A RELOCATION LINE BELONGS TO THE INSTRUCTION ABOVE IT, and it
            # must be applied THERE.  The first version of this did it in a
            # second loop over the whole output, so `rows[-1]` was the
            # function's LAST instruction every time -- usually a `ret` with
            # no operands -- and the normaliser silently did nothing at all
            # for 210 relocated instructions across the tree while reporting
            # a clean run.  Findings 134 and 2401: a detector that cannot be
            # seen to fire has not been shown to work.
            #
            r = RELOC.match(line)
            if r and rows:
                tag = "@" + r.group(3)
                sub, k = re.subn(r"0x[0-9a-f]+|\$0x[0-9a-f]+", tag, rows[-1][1])
                #
                # A CALL HAS NO NUMERIC LITERAL LEFT TO REPLACE -- its operand
                # is a bare address already rewritten to `.+N` above -- so a
                # substitution alone drops the target on the floor, and two
                # calls to DIFFERENT functions compare equal.  That is not
                # hypothetical: `V90PreFilter`'s destructors call
                # `FloatFIR::~FloatFIR` D2 in the blob and D1 in ours, and
                # both were being certified as "same instructions and
                # operands".  Append where nothing was replaced.
                #
                rows[-1][1] = sub if k else (rows[-1][1] + " " + tag).strip()
            continue
        at = int(m.group(1), 16)
        if base is None:
            base = at
        body = line.split("\t", 1)[1].strip()
        body = body.split("#", 1)[0].strip()
        body = re.sub(r"<[^>]*>", "", body).strip()
        parts = body.split(None, 1)
        ops = parts[1].strip() if len(parts) > 1 else ""
        #
        # A BRANCH TARGET IS AN ABSOLUTE ADDRESS AND THE TWO OBJECTS PUT THE
        # FUNCTION AT DIFFERENT OFFSETS, so `jmp 7e321` and `jmp f1` are the
        # same jump printed twice.  Comparing them literally scored 310
        # functions as different that differ in nothing at all.  Rewrite a
        # bare hex operand as an offset from the function's own first
        # instruction, which is the same number on both sides.
        #
        if re.fullmatch(r"[0-9a-f]+", ops):
            ops = ".%+d" % (int(ops, 16) - base)
        rows.append([parts[0], ops])
        continue
    #
    # A RELOCATED OPERAND'S PRINTED NUMBER IS A PLACEHOLDER, and the two
    # objects do not use the same one: the blob prints
    # `movswl 0x5740(%eax,%eax,1)` where ours prints `0x0(...)`, because the
    # blob's addend rides inline against a SECTION symbol and ours is a named
    # symbol with a zero addend (finding 604).  Comparing the printed
    # displacement scores that as a difference in code where there is none.
    # Replace every numeric literal in a relocated instruction's operands with
    # the relocation's TARGET, so two instructions relocated against the same
    # thing compare equal and two relocated against different things do not.
    #
    return [tuple(r) for r in rows]


def alpha_equal(x, y):
    """Same instructions and operands up to a CONSISTENT register bijection.

    This is grade 1 and it is STRICTER than `compare.py`, which drops operands
    altogether -- there, `mov $1,%eax` and `mov $2,%ebx` both read as `mov`.
    Here every immediate and displacement must match exactly and only the
    register NAMES may differ, under one bijection held for the whole function
    (%eax<->%edx implies %al<->%dl, so tokens are folded to their 32-bit
    family and the width must still agree).  `%esp` and `%ebp` are pinned to
    themselves: the frame is not a free choice.
    """
    if len(x) != len(y):
        return False
    fwd, rev = {"esp": "esp", "ebp": "ebp"}, {"esp": "esp", "ebp": "ebp"}
    for (mx, ox), (my, oy) in zip(x, y):
        if mx != my:
            return False
        rx, ry = REGTOK.findall(ox), REGTOK.findall(oy)
        if len(rx) != len(ry):
            return False
        for u, v in zip(rx, ry):
            fu, fv = REG32.get(u), REG32.get(v)
            if fu is None or fv is None:          # segment or x87 -- exact
                if u != v:
                    return False
                continue
            if len(u) != len(v):                  # same access width
                return False
            if fwd.setdefault(fu, fv) != fv or rev.setdefault(fv, fu) != fu:
                return False
        if REGTOK.sub("%r", ox) != REGTOK.sub("%r", oy):
            return False
    return True


def verdict(a, ra, b, rb):
    if a is None or b is None:
        return "NODATA", 0
    if len(a) != len(b):
        return "SIZE", abs(len(a) - len(b))
    masked = set()
    for off in set(ra) | set(rb):
        masked.update(range(off, min(off + 4, len(a))))
    diff = sum(1 for i in range(len(a)) if i not in masked and a[i] != b[i])
    if diff:
        return "BYTES", diff
    if ra != rb:
        #
        # A SECTION-RELATIVE RELOCATION AND A NAMED-SYMBOL ONE CAN NAME THE
        # SAME THING.  `objdump` prints the blob's string and table references
        # as `R_386_32 .rodata` with the offset as an inline addend, and ours
        # as `R_386_32 v8_costab` -- finding 604.  Comparing the printed names
        # scores that as a differing target when nothing differs, so a pair
        # where one side names a section is reported as UNRESOLVED and NOT as
        # a difference.  Resolving it properly needs the addend, which
        # `objdump -dr` does not print.
        #
        sections = (".text", ".rodata", ".data", ".bss")
        hard = soft = 0
        for k in set(ra) | set(rb):
            x, y = ra.get(k), rb.get(k)
            if x == y:
                continue
            if (x and x[1].startswith(sections)) or (y and y[1].startswith(sections)):
                soft += 1
            else:
                hard += 1
        if hard:
            return "RELOC", hard
        if soft:
            return "UNRESOLVED", soft
    return "EXACT", 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list-exact", action="store_true")
    ap.add_argument("--limit", type=int, default=25)
    a = ap.parse_args()

    blob = sizes(BLOB)
    if not blob:
        sys.exit("byteident.py: NO SYMBOLS read from the blob at %s.\n"
                 "  Every count below would be computed against NOTHING and\n"
                 "  would render as a clean zero.  From a worktree, BLOB must\n"
                 "  be explicit.  Findings 2400, 2401." % BLOB)
    ours = {}
    for o in sorted(glob.glob(os.path.join(OURS, "*.o"))):
        for k, v in sizes(o).items():
            ours.setdefault(k, o)
    if not ours:
        sys.exit("byteident.py: no objects in %s -- run "
                 "tools/toolchain/build.sh first." % OURS)

    common = sorted(k for k in ours if k in blob)
    if not common:
        sys.exit("byteident.py: blob and %s share NO symbols; the denominator "
                 "is zero, which is not a score." % OURS)

    buckets = {"EXACT": [], "UNRESOLVED": [], "REGALLOC": [], "RELOC": [],
               "BYTES": [], "SIZE": [], "NODATA": []}
    for k in common:
        ab, ar = body(BLOB, k)
        bb, br = body(ours[k], k)
        v, n = verdict(ab, ar, bb, br)
        #
        # GRADE 1 IS CHECKED ONLY WHERE GRADE 0 FAILED.  A byte-identical
        # function is trivially alpha-equal and asking again costs two
        # disassemblies for no information.
        #
        #
        # RELOC IS NOT A CANDIDATE FOR GRADE 1.  A differing relocation target
        # means the function calls or reads something else, which no amount of
        # register renaming makes equivalent; letting `alpha_equal` overrule it
        # promoted two destructors that call a different symbol.
        #
        if v not in ("EXACT", "UNRESOLVED", "NODATA", "RELOC"):
            if alpha_equal(insns(BLOB, k), insns(ours[k], k)):
                v, n = "REGALLOC", 0
        buckets[v].append((n, blob[k], k))

    n = len(common)
    print("  blob    : %s" % BLOB)
    print("  ours    : %s\n" % OURS)
    print("Positional byte identity over %d symbol(s) both define.\n" % n)
    ex = len(buckets["EXACT"])
    un = len(buckets["UNRESOLVED"])
    ra = len(buckets["REGALLOC"])
    print("  grade 0  EXACT      same bytes, same places  : %4d  (%.1f%%)"
          % (ex, 100.0 * ex / n))
    print("           UNRESOLVED as EXACT, but a section  : %4d" % un)
    print("                      relocation cannot be")
    print("                      compared by name (604)")
    print("  grade 1  REGALLOC   same instructions and    : %4d" % ra)
    print("                      operands, one consistent")
    print("                      register bijection")
    print("           ------------------------------------------")
    print("           grade 0 or 1                        : %4d  (%.1f%%)"
          % (ex + un + ra, 100.0 * (ex + un + ra) / n))
    print("  RELOC   -- bytes agree, a target differs : %4d" % len(buckets["RELOC"]))
    print("  BYTES   -- same size, bytes differ       : %4d" % len(buckets["BYTES"]))
    print("  SIZE    -- different size                : %4d" % len(buckets["SIZE"]))
    if buckets["NODATA"]:
        print("  NODATA  -- could not be disassembled     : %4d" % len(buckets["NODATA"]))

    if a.list_exact:
        print("\nEXACT:")
        for _, sz, k in sorted(buckets["EXACT"], key=lambda r: -r[1]):
            print("  %6d  %s" % (sz, k))
        return 0

    for name, label in (("RELOC", "differing relocation target(s)"),
                        ("BYTES", "differing byte(s)")):
        rows = sorted(buckets[name], key=lambda r: r[0])[:a.limit]
        if rows:
            print("\n%s, closest first -- %s:" % (name, label))
            for d, sz, k in rows:
                print("  %5d of %5d  %s" % (d, sz, k))
    return 0


if __name__ == "__main__":
    sys.exit(main())
