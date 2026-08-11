#!/usr/bin/env python3
"""
blobfix.py -- repair defects in `dsplibs.o` WITHOUT modifying `dsplibs.o`.

WHY THIS EXISTS

The reconstruction is not finished, so anything shipping today still links the
blob.  Two of the defects in `docs/deviations.md` are in the blob's DATA, not
its code -- a table declared one entry shorter than its own index expression
can reach:

    D1  FPM_sqrt_table   192 entries, index reaches 192   (harmless by luck)
    D4  FPM_div_table    128 entries, index reaches 128   (returns 0; HARMFUL)

Neither can be repaired by patching instructions, and that is the point of
this tool.  The correct value at the missing index does not exist anywhere in
the object, so no rearrangement of the code can produce it:

    sqrt_table[192] must be 32768;  the last real entry, 191, is 32703.
    div_table[128]  must be 16384;  what the object reads there is 0.

A clamp -- the repair a code patch can express, and the one `FPM_sqrt_dp`
actually has -- changes the answer on every input that reaches the end.  So
the only faithful fix is to make the table longer, and a table cannot grow
inside a linked object.  It can be REPLACED: weaken the blob's definition and
link a longer one in front of it.

WHAT IT DOES

    tools/blobfix.py generate --outdir DIR --fix D1 [--fix D4]

        1. reads the blob's own table out of `.rodata`
        2. regenerates every existing entry from the stated formula and
           ABORTS unless all of them match -- the generator has to be shown
           to reproduce the object before its extra entry is believed
        3. writes DIR/blobfix.c, a strong definition one entry longer
        4. runs `objcopy --weaken-symbol=` on the blob into DIR/dsplibs_fixed.o
        5. VERIFIES that output: every section's bytes identical, every
           relocation identical once resolved to (offset, type, symbol name),
           every symbol identical except the named ones' GLOBAL -> WEAK.
           So "no instruction was touched" is measured, not asserted.

    tools/blobfix.py checklink BINARY --fix D1 [--fix D4]

        Reads the LINKED OUTPUT back.  For each site that indexes the table
        it disassembles the instruction, extracts the displacement the linker
        actually wrote, and checks it equals the address of the replacement
        table -- then checks the highest index the expression can produce is
        inside that table's size.

        This is the step that exists because of the fork's dead patch
        (`docs/forkblob.md`): a patch at `rebuildJMSequence+0x136` that is
        overwritten two instructions later, applied and inert, with nothing
        in the build that would ever say so.  A fix here is not believed
        because it was applied; it is believed because the output was read.

WHAT IT DOES NOT DO

`slmodemd/dsplibs.o` is never opened for writing.  Everything lands in
--outdir.  Both fixes are opt-in and `make phase` runs with neither.
"""

import argparse
import hashlib
import math
import os
import re
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import elfinfo                                          # noqa: E402


# ---------------------------------------------------------------------------
# The fix register.
#
# `entries` and `reach` are the two numbers the defect is made of: how long
# the table is, and how far the index expression can go.  A fix exists exactly
# when reach >= entries.  Both are stated here with their derivation, and
# `entries` is re-read from the object at run time and cross-checked.
# ---------------------------------------------------------------------------

FIXES = {
    "D1": dict(
        symbol="FPM_sqrt_table",
        entries=192,
        reach=192,
        # index = ((mantissa + 64) >> 7) - 64 over a normalised mantissa in
        # [0x4000, 0x7fff], so entry i is the Q15 square root of (i+64)/256.
        #
        # TWO CONSUMERS, and `reach` covers both.  `FPM_sqrt` is the one that
        # overruns; `FPM_sqrt_dp` indexes the same table and clamps at 191
        # (`cmp $0xbf`), so it cannot reach 192 whatever its argument, which
        # is also why the longer table is provably a no-op for it.
        formula="floor(32768 * sqrt((i + 64) / 256))",
        gen=lambda i: int(math.floor(32768 * math.sqrt((i + 64) / 256.0))),
        behaviour="none -- the blob reads FPM_div_table[0], which is already "
                  "32768, the value the formula gives",
        deviation="D1, finding 13",
    ),
    "D4": dict(
        symbol="FPM_div_table",
        entries=128,
        reach=128,
        # index = ((mantissa + 0x80) >> 8) - 0x80 over a normalised mantissa
        # in [0x8000, 0xffff], so entry i is the Q14 reciprocal of
        # (i+128)/256, i.e. 2^30 / ((i + 0x80) * 0x100).
        #
        # TWO CONSUMERS, and `reach` covers both -- checked, not assumed.
        # `FPM_div_32` (0xa6c90) forms the SAME expression from `x >> 16`
        # after normalising until bit 31 is set, so its mantissa is in
        # [0x8000, 0xffff] too and its index runs 0..128 exactly.  It has no
        # clamp, and unlike `FPM_sqrt_dp` it has no truncation that could
        # drive the index negative, because the mantissa is taken from an
        # already-normalised 32-bit value.  It has no reconstruction either,
        # so `test/blobfix` is the only thing in this tree that drives it.
        formula="trunc(2^30 / ((i + 0x80) * 0x100))",
        gen=lambda i: (1 << 30) // ((i + 0x80) * 0x100),
        behaviour="CHANGES BEHAVIOUR -- the blob reads FPM_xor_table[0], "
                  "which is 0, where the formula gives 16384.  A reciprocal "
                  "of zero silences the AGC block that asked for it",
        deviation="D4, finding 40",
    ),
}

ELEM = 2                        # every table here is `unsigned short`


def die(msg):
    sys.stderr.write("blobfix: %s\n" % msg)
    sys.exit(1)


def run(args):
    p = subprocess.run(args, capture_output=True, text=True)
    if p.returncode != 0:
        die("%s failed:\n%s" % (" ".join(args), p.stderr))
    return p.stdout


# ---------------------------------------------------------------------------
# Reading an object
# ---------------------------------------------------------------------------

def section_bytes(path, sec):
    with open(path, "rb") as f:
        f.seek(sec.off)
        return f.read(sec.size)


def find_symbol(syms, name):
    hits = [s for s in syms if s.name == name and s.ndx.isdigit()]
    if len(hits) != 1:
        die("expected exactly one definition of %s, found %d" %
            (name, len(hits)))
    return hits[0]


def read_table(path, sections, sym, count):
    sec = sections[sym.ndx]
    with open(path, "rb") as f:
        f.seek(sec.off + sym.value - sec.addr)
        raw = f.read(count * ELEM)
    return list(struct.unpack("<%dH" % count, raw))


def resolved_relocs(path):
    """Every relocation as (section, offset, type, symbol-name, sym-value).

    Deliberately NOT the raw bytes: `objcopy` renumbers the symbol table, so
    the `r_info` field legitimately differs between input and output while
    the relocation means exactly the same thing.  Comparing raw `.rel` bytes
    would report thousands of false differences and hide any real one.
    """
    out = []
    cur = None
    for line in run(["readelf", "-rW", path]).splitlines():
        m = re.match(r"Relocation section '(\S+)'", line)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r"\s*([0-9a-f]{8})\s+([0-9a-f]+)\s+(\S+)\s+"
                     r"([0-9a-f]+)\s*(.*)$", line)
        if m and cur:
            off, _info, typ, val, name = m.groups()
            out.append((cur, int(off, 16), typ, name.strip(), int(val, 16)))
    return out


# ---------------------------------------------------------------------------
# generate
# ---------------------------------------------------------------------------

def emit_c(path, tables):
    lines = [
        "/*",
        " * blobfix.c -- GENERATED by tools/blobfix.py.  Do not edit.",
        " *",
        " * Strong replacements for tables `dsplibs.o` declares one entry",
        " * shorter than its own index expression can reach.  Linked ahead of",
        " * the weakened blob, these win, and the read that used to leave the",
        " * table lands inside it.",
        " *",
        " * Every entry below the last is REGENERATED from the formula and",
        " * was checked against the blob's own bytes at generation time; the",
        " * last is the same formula continued.  Nothing here is copied from",
        " * an address the object did not intend to be read.",
        " */",
        "",
    ]
    for fix_id, f, vals, sentinel in tables:
        lines += [
            "/*",
            " * %s (%s): %s" % (f["symbol"], fix_id, f["deviation"]),
            " *   blob: %d entries, index reaches %d" % (f["entries"],
                                                         f["reach"]),
            " *   entry i = %s" % f["formula"],
            " *   effect: %s" % f["behaviour"],
            " */",
        ]
        if sentinel is not None:
            lines.append("/* NEGATIVE CONTROL: last entry replaced by the "
                         "sentinel 0x%04x. */" % sentinel)
        lines.append("const unsigned short %s[%d] = {" %
                     (f["prefixed"], len(vals)))
        for i in range(0, len(vals), 8):
            lines.append("\t" + ", ".join("%5d" % v for v in vals[i:i + 8])
                         + ",")
        lines += ["};", ""]
    with open(path, "w") as fh:
        fh.write("\n".join(lines))


def verify_objcopy(src, dst, weakened):
    """Prove the objcopy changed the linkage of `weakened` and NOTHING else."""
    ssec = elfinfo.read_sections(src)
    dsec = elfinfo.read_sections(dst)
    sbyname = {s.name: s for s in ssec.values()}
    dbyname = {s.name: s for s in dsec.values()}

    problems = []
    if set(sbyname) != set(dbyname):
        problems.append("section names differ: %s" %
                        (set(sbyname) ^ set(dbyname)))

    for name in sorted(set(sbyname) & set(dbyname)):
        a, b = sbyname[name], dbyname[name]
        if name.startswith(".rel") or name in (".symtab", ".strtab",
                                               ".shstrtab"):
            continue                    # compared semantically below
        if a.size != b.size or a.addr != b.addr:
            problems.append("%s: size/addr changed" % name)
            continue
        if a.type == "NOBITS":
            continue
        if section_bytes(src, a) != section_bytes(dst, b):
            problems.append("%s: CONTENTS CHANGED" % name)

    ra, rb = resolved_relocs(src), resolved_relocs(dst)
    if ra != rb:
        n = sum(1 for x, y in zip(ra, rb) if x != y) + abs(len(ra) - len(rb))
        problems.append("%d relocations differ once resolved" % n)

    def key(s):
        return (s.name, s.value, s.size, s.type, s.ndx)

    sa = {}
    for s in elfinfo.read_symbols(src):
        sa.setdefault(key(s), []).append(s.bind)
    sb = {}
    for s in elfinfo.read_symbols(dst):
        sb.setdefault(key(s), []).append(s.bind)

    #
    # A MODERN BINUTILS DROPS `SECTION` SYMBOLS FOR `.rel*` AND THE THREE
    # METADATA SECTIONS, and the blob was produced by binutils of 2003 which
    # emitted them.  `objcopy` rewrites the symbol table wholesale, so they
    # disappear whatever the operation was -- the fork's shipped blob lost 56
    # of them for the same reason (`docs/forkblob.md`).  They name no code and
    # no data and carry no value or size, so this is accepted BY NAME rather
    # than by a loosened comparison: anything else vanishing still fails.
    #
    def is_metadata_section_sym(k):
        name, value, size, typ, _ndx = k
        return (typ == "SECTION" and value == 0 and size == 0 and
                (name.startswith(".rel") or
                 name in (".symtab", ".strtab", ".shstrtab")))

    dropped = set(sa) - set(sb)
    kept_extra = set(sb) - set(sa)
    stray = [k for k in dropped if not is_metadata_section_sym(k)]
    if stray:
        problems.append("%d symbols vanished that are not `.rel*` SECTION "
                        "entries, first %r" % (len(stray), sorted(stray)[0]))
    if kept_extra:
        problems.append("%d symbols appeared: %r" %
                        (len(kept_extra), sorted(kept_extra)[:3]))
    changed = []
    for k in set(sa) & set(sb):
        if sorted(sa[k]) != sorted(sb[k]):
            changed.append((k[0], sorted(sa[k]), sorted(sb[k])))
    expected = {n: (["GLOBAL"], ["WEAK"]) for n in weakened}
    for name, before, after in changed:
        if expected.get(name) != (before, after):
            problems.append("unexpected binding change on %s: %s -> %s" %
                            (name, before, after))
    for name in weakened:
        if not any(c[0] == name for c in changed):
            problems.append("%s was NOT weakened" % name)

    text = sbyname.get(".text")
    print("  .text   %s  %d bytes  (unchanged: %s)" % (
        hashlib.md5(section_bytes(dst, dbyname[".text"])).hexdigest(),
        text.size,
        section_bytes(src, text) == section_bytes(dst, dbyname[".text"])))
    print("  .rodata %s  %d bytes  (unchanged: %s)" % (
        hashlib.md5(section_bytes(dst, dbyname[".rodata"])).hexdigest(),
        sbyname[".rodata"].size,
        section_bytes(src, sbyname[".rodata"]) ==
        section_bytes(dst, dbyname[".rodata"])))
    print("  relocations: %d, all identical once resolved: %s" %
          (len(ra), ra == rb))
    print("  symbols changed: %d (%s)" %
          (len(changed), ", ".join("%s %s->%s" % (c[0], c[1][0], c[2][0])
                                   for c in changed) or "none"))
    print("  symbols dropped: %d, all `.rel*`/metadata SECTION entries: %s" %
          (len(dropped), not stray))

    if problems:
        for p in problems:
            sys.stderr.write("blobfix: VERIFY FAILED: %s\n" % p)
        sys.exit(1)
    print("  verified: linkage changed, no byte of code or data touched")


def cmd_generate(args):
    blob = args.blob
    sections = elfinfo.read_sections(blob)
    syms = elfinfo.read_symbols(blob)

    sentinels = {}
    for spec in args.sentinel:
        k, _, v = spec.partition("=")
        if k not in FIXES:
            die("unknown fix %r in --sentinel" % k)
        sentinels[k] = int(v, 0)

    tables, weakened = [], []
    for fix_id in args.fix:
        f = dict(FIXES[fix_id])
        f["prefixed"] = args.prefix + f["symbol"]
        sym = find_symbol(syms, f["prefixed"])
        if sym.size != f["entries"] * ELEM:
            die("%s is %d bytes, expected %d entries of %d" %
                (f["prefixed"], sym.size, f["entries"], ELEM))

        blob_vals = read_table(blob, sections, sym, f["entries"])
        mism = [(i, blob_vals[i], f["gen"](i))
                for i in range(f["entries"]) if blob_vals[i] != f["gen"](i)]
        if mism:
            die("%s: generator does not reproduce the object -- %d of %d "
                "entries differ, first %r.  Refusing to invent the missing "
                "entry from a formula the object disagrees with."
                % (f["prefixed"], len(mism), f["entries"], mism[0]))

        vals = [f["gen"](i) for i in range(f["reach"] + 1)]
        print("%s  %s: %d entries reproduced exactly; entry %d = %d  [%s]" % (
            fix_id, f["prefixed"], f["entries"], f["reach"], vals[f["reach"]],
            f["formula"]))
        if fix_id in sentinels:
            vals[f["reach"]] = sentinels[fix_id]
            print("      NEGATIVE CONTROL: entry %d overwritten with 0x%04x"
                  % (f["reach"], sentinels[fix_id]))
        tables.append((fix_id, f, vals, sentinels.get(fix_id)))
        weakened.append(f["prefixed"])

    os.makedirs(args.outdir, exist_ok=True)
    csrc = os.path.join(args.outdir, "blobfix.c")
    emit_c(csrc, tables)
    print("wrote %s" % csrc)

    out = os.path.join(args.outdir, "dsplibs_fixed.o")
    run(["objcopy"] + ["--weaken-symbol=%s" % n for n in weakened] +
        [blob, out])
    print("wrote %s" % out)
    verify_objcopy(blob, out, weakened)


# ---------------------------------------------------------------------------
# checklink -- read the linked output back
# ---------------------------------------------------------------------------

def addr_to_off(sections, addr):
    for s in sections.values():
        if s.type != "NOBITS" and s.addr <= addr < s.addr + s.size:
            return s.off + (addr - s.addr)
    die("address 0x%x is in no section" % addr)


def consumer_sites(blob, symbol):
    """Where the blob indexes `symbol`, as (function, offset-in-function).

    Derived from the blob's own relocations, not from a list written here:
    a site nobody remembered would otherwise go unchecked.
    """
    sections = elfinfo.read_sections(blob)
    funcs = [s for s in elfinfo.read_symbols(blob)
             if s.type == "FUNC" and s.ndx.isdigit()]
    sites = []
    for sec, off, typ, name, _val in resolved_relocs(blob):
        if name != symbol:
            continue
        if typ != "R_386_32":
            die("%s has an unexpected relocation type %s" % (symbol, typ))
        target = sec[len(".rel"):]
        if target != ".text":
            die("%s is referenced from %s, not .text" % (symbol, target))
        owner = [f for f in funcs
                 if f.value <= off < f.value + f.size and
                 sections[f.ndx].name == ".text"]
        if len(owner) != 1:
            die("relocation at 0x%x is inside %d functions" % (off,
                                                               len(owner)))
        sites.append((owner[0].name, off - owner[0].value))
    return sites


def disasm_at(binary, addr):
    txt = run(["objdump", "-d", "--start-address=0x%x" % (addr - 16),
               "--stop-address=0x%x" % (addr + 16), binary])
    best = None
    for line in txt.splitlines():
        m = re.match(r"\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(\S.*)$", line)
        if not m:
            continue
        a = int(m.group(1), 16)
        n = len(m.group(2).split())
        if a <= addr < a + n:
            best = (a, m.group(2).strip(), m.group(3).strip())
    return best


def cmd_checklink(args):
    sections = elfinfo.read_sections(args.binary)
    syms = elfinfo.read_symbols(args.binary)
    blob_sections = elfinfo.read_sections(args.blob)
    bfunc = {s.name: s for s in elfinfo.read_symbols(args.blob)
             if s.type == "FUNC" and s.ndx.isdigit()}
    image = open(args.binary, "rb").read()
    blob_image = open(args.blob, "rb").read()
    ok = True

    for fix_id in args.fix:
        f = FIXES[fix_id]
        name = args.prefix + f["symbol"]
        tsym = find_symbol(syms, name)
        need = (f["reach"] + 1) * ELEM
        print("%s  %s in %s: 0x%08x, %d bytes (%d entries)" %
              (fix_id, name, os.path.basename(args.binary), tsym.value,
               tsym.size, tsym.size // ELEM))

        if tsym.size < need:
            print("      FAIL: index reaches %d, needs %d bytes, table is %d"
                  % (f["reach"], need, tsym.size))
            ok = False
        else:
            print("      index reaches %d -> last byte read 0x%08x, "
                  "table ends 0x%08x  IN BOUNDS" %
                  (f["reach"], tsym.value + need - 1, tsym.value + tsym.size))

        for func, delta in consumer_sites(args.blob, name):
            fsym = find_symbol(syms, func)
            site = fsym.value + delta
            disp = struct.unpack_from("<I", image,
                                      addr_to_off(sections, site))[0]
            ins = disasm_at(args.binary, site)
            print("      %-12s +0x%03x  %s" %
                  (func, delta, "%s\t%s" % (ins[1], ins[2]) if ins else "??"))
            if ins is None or ("0x%x" % disp) not in ins[2]:
                print("      FAIL: disassembly does not show 0x%x" % disp)
                ok = False
            elif disp != tsym.value:
                print("      FAIL: linker wrote 0x%08x, table is at 0x%08x"
                      % (disp, tsym.value))
                ok = False

            #
            # THE INSTRUCTION ITSELF MUST BE THE BLOB'S, byte for byte, with
            # only the relocated field changed.  This is the check that says
            # the fix is a linkage change and not a code patch, and it is
            # narrowed to the one instruction on purpose: a function's other
            # bytes may legitimately differ, because any other relocation in
            # it -- `FPM_div` calls `dsplibs_debug_printf` -- has also been
            # resolved by the linker.  Comparing the whole prologue reports
            # that as tampering, which it is not.
            #
            if ins is not None:
                start, hexb, _ = ins
                nbytes = len(hexb.split())
                got = bytes(int(h, 16) for h in hexb.split())
                boff = addr_to_off(blob_sections,
                                   bfunc[func].value + (start - fsym.value))
                want = blob_image[boff:boff + nbytes]
                lo = delta - (start - fsym.value)
                mask = slice(lo, lo + 4)
                if len(got) != len(want) or \
                        got[:mask.start] != want[:mask.start] or \
                        got[mask.stop:] != want[mask.stop:]:
                    print("      FAIL: instruction differs from the blob's "
                          "outside the relocated field: %s vs %s" %
                          (got.hex(" "), want.hex(" ")))
                    ok = False

    print("checklink: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


# ---------------------------------------------------------------------------
# tamper -- make `checklink` fail on purpose
#
# A checker nobody has seen fail is a checker that might be reporting "clean"
# because it is broken; `extcheck.py` printed "(none)" through four dead
# versions before anyone noticed (CLAUDE.md, finding 134's argument).  So the
# acceptance test damages a copy of the fixed binary in each of the two ways
# `checklink` claims to catch, and requires it to catch them.
#
# `displacement` retargets the table by two bytes -- one entry -- which is
# what a fix aimed at the wrong symbol would look like.
# `opcode` alters a byte of the instruction outside the relocated field,
# which is what an unannounced code patch would look like.
# ---------------------------------------------------------------------------

def cmd_tamper(args):
    sections = elfinfo.read_sections(args.binary)
    syms = elfinfo.read_symbols(args.binary)
    image = bytearray(open(args.binary, "rb").read())

    f = FIXES[args.fix]
    func, delta = consumer_sites(args.blob, args.prefix + f["symbol"])[0]
    site = find_symbol(syms, func).value + delta
    off = addr_to_off(sections, site)

    if args.mode == "displacement":
        disp = struct.unpack_from("<I", image, off)[0]
        struct.pack_into("<I", image, off, disp + ELEM)
        what = "displacement 0x%x -> 0x%x" % (disp, disp + ELEM)
    else:
        # The byte before the displacement is the ModRM/SIB tail of the
        # instruction, never the relocated field.
        image[off - 1] ^= 0x01
        what = "instruction byte at %s+0x%x-1 flipped" % (func, delta)

    with open(args.out, "wb") as fh:
        fh.write(image)
    os.chmod(args.out, 0o755)
    print("tampered %s: %s" % (args.out, what))


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--blob",
                    default=os.environ.get("BLOB", "../slmodemd/dsplibs.o"))
    sub = ap.add_subparsers(dest="cmd", required=True)

    g = sub.add_parser("generate")
    g.add_argument("--outdir", required=True)
    g.add_argument("--fix", action="append", choices=sorted(FIXES),
                   required=True)
    g.add_argument("--prefix", default="",
                   help="symbol prefix, for a ref_-renamed reference object")
    g.add_argument("--sentinel", action="append", default=[],
                   metavar="Dn=VALUE",
                   help="replace the new last entry with VALUE (test only)")
    g.set_defaults(func=cmd_generate)

    c = sub.add_parser("checklink")
    c.add_argument("binary")
    c.add_argument("--fix", action="append", choices=sorted(FIXES),
                   required=True)
    c.add_argument("--prefix", default="")
    c.set_defaults(func=cmd_checklink)

    t = sub.add_parser("tamper")
    t.add_argument("binary")
    t.add_argument("--fix", choices=sorted(FIXES), required=True)
    t.add_argument("--mode", choices=("displacement", "opcode"),
                   required=True)
    t.add_argument("--prefix", default="")
    t.add_argument("-o", "--out", required=True)
    t.set_defaults(func=cmd_tamper)

    args = ap.parse_args()
    sys.exit(args.func(args) or 0)


if __name__ == "__main__":
    main()
