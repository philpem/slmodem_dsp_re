#!/usr/bin/env python3
"""
Disassemble a range of dsplibs.o with relocations attached to their operands.

WHY THIS EXISTS

`objdump -d -r` prints relocations on their own lines, indented with tabs,
*after* the instruction they belong to.  Every convenient way of trimming that
output -- a `grep -v` for noise, an `awk` range on the address column, a `sed`
window -- drops them, because they do not look like instruction lines.  The
result is that

        7df7f:  b9 00 00 00 00    mov    $0x0,%ecx
                7df80: R_386_32   CP_276_504_a

reads as `ecx = 0` instead of `ecx = &CP_276_504_a`, and a table of pointers
becomes a table of zeros or, worse, of plausible small integers.

That mistake has been made three times in this reconstruction:

  - the toneiir configuration's three coefficient pointers read as integers
    near 25000 (finding F46)
  - the CP_* tables read as unreferenced (retracted at D10)
  - `cadence_create`'s bank-3 fallback read as installing NULL pointers,
    which would have been a serious defect report about a crash that does
    not happen

So: this tool folds each relocation into the instruction line, and there is
nothing left to accidentally filter out.  Use it instead of raw objdump when
reading anything that might touch a table.

USAGE

    dis.py <obj> <symbol>              whole function
    dis.py <obj> 0x7df75 0x7dfab       an address range
    dis.py <obj> <symbol> --plain      no noise suppression
    dis.py <obj> 0x0 0x40 --section .gnu.linkonce.t.name

Symbols select their defining ELF section. Numeric ranges default to .text;
use --section for another section. Ambiguous local names need a distinguishing
--section or a numeric range.

By default it drops padding (`nop`, `lea 0x0(%esi),%esi` and friends), because
those never carry relocations.  Nothing else is ever hidden.
"""

import argparse
import re
import subprocess
import sys

#
# The x87 operations whose AT&T mnemonic can be the reverse of what the bytes
# encode.  Only the popping DE forms actually flip -- the D8 register forms
# agree -- but the whole family is listed so a new spelling cannot slip past,
# and the annotation only fires when the two renderings genuinely differ.
#
REVERSIBLE = {"fdivp", "fdivrp", "fsubp", "fsubrp",
              "fdiv", "fdivr", "fsub", "fsubr"}

# Alignment padding gcc emits between blocks. A matching shape is suppressed
# only after checking that it carries no relocation.
PADDING = re.compile(
    r'\t(nop|xchg\s+%ax,%ax|lea\s+0x0\((%e[a-z]{2})(,%eiz,1)?\),\2)\s*$')

RELOC = re.compile(r'^\s+([0-9a-f]+):\s+(R_386_\S+)\s+(\S+)(?:([+-]0x[0-9a-f]+))?')
INSN = re.compile(r'^\s*([0-9a-f]+):\t')
SECTION = re.compile(r'^\s*\[\s*(\d+)\]\s+(\S+)')
BYTES = re.compile(r'^\s*[0-9a-f]+:\s+((?:[0-9a-f]{2}\s)+)')


def sections(obj):
    out = subprocess.check_output(['readelf', '--sections', '--wide', obj], text=True)
    return {int(m[1]): m[2] for line in out.splitlines()
            if (m := SECTION.match(line))}


def symbol_range(obj, name, requested_section=None):
    names = sections(obj)
    out = subprocess.check_output(['readelf', '--syms', '--wide', obj], text=True)
    matches = set()
    for line in out.splitlines():
        f = line.split()
        if len(f) == 8 and f[7] == name and f[6].isdigit() and int(f[2]):
            section = names[int(f[6])]
            if requested_section is None or section == requested_section:
                lo = int(f[1], 16)
                matches.add((section, lo, lo + int(f[2])))
    if not matches:
        raise ValueError("no such symbol with a size: %s" % name)
    if len(matches) != 1:
        raise ValueError("ambiguous symbol %s; select --section or an address range" % name)
    return matches.pop()


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("obj")
    ap.add_argument("start", help="symbol name or start address")
    ap.add_argument("stop", nargs="?", help="stop address")
    ap.add_argument("--plain", action="store_true",
                    help="keep padding instructions")
    ap.add_argument("--section", help="ELF section (numeric ranges default to .text)")
    args = ap.parse_args()

    try:
        if args.stop is None and not args.start.startswith("0x"):
            section, lo, hi = symbol_range(args.obj, args.start, args.section)
        else:
            section = args.section or '.text'
            if section not in sections(args.obj).values():
                raise ValueError("no such section: %s" % section)
            lo = int(args.start, 0)
            hi = int(args.stop, 0) if args.stop else lo + 0x100
        if hi <= lo:
            raise ValueError("stop address must follow start address")
    except (ValueError, subprocess.CalledProcessError) as error:
        ap.error(str(error))

    scope = ['-j', section, '--start-address=0x%x' % lo,
             '--stop-address=0x%x' % hi, args.obj]

    # -w moves relocations onto instruction lines on recent binutils. Keep
    # separate relocation records while preventing long instruction wrapping.
    raw = subprocess.check_output(['objdump', '-d', '-r', '--insn-width=16'] + scope,
                                   text=True)

    #
    # THE REVERSIBLE x87 FAMILY IS ANNOTATED WITH ITS INTEL MNEMONIC, because
    # the AT&T one is its own opposite and reading it straight has cost this
    # project real time (finding F245).  `de f1` prints `fdivp` and IS FDIVRP:
    # ST(1) = ST(0)/ST(1).
    #
    # MEASURED, not assumed, and the measurement is the point: binutils 2.15
    # in the period container and 2.42 natively print these IDENTICALLY in
    # AT&T syntax, so upgrading does not fix it and never would have.  What
    # does fix it is `-M intel`, which renders the same bytes the way the
    # Intel manual names them.  So we ask objdump twice and print both.
    #
    intel = subprocess.check_output(['objdump', '-d', '--insn-width=16', '-M', 'intel'] + scope,
                                    text=True)
    intel_at = {}
    for line in intel.splitlines():
        m = re.match(r"\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )+\s*(\S+)", line)
        if m:
            intel_at[int(m.group(1), 16)] = m.group(2)

    # Collect relocations by the address they annotate, then attach each to
    # the instruction that contains it.
    relocs = {}
    lines = raw.splitlines()
    for line in lines:
        m = RELOC.match(line)
        if m:
            addend = m.group(4) or ""
            at = int(m.group(1), 16)
            if lo <= at < hi:
                relocs[at] = "%s %s%s" % (m.group(2), m.group(3), addend)

    out = []
    starts = []
    for line in lines:
        m = INSN.match(line)
        if m:
            starts.append((int(m.group(1), 16), line))

    shown = set()
    for addr, line in starts:
        encoded = BYTES.match(line)
        if not encoded:
            raise ValueError("instruction has no decoded bytes: %s" % line)
        end = addr + len(encoded[1].split())
        tags = [relocs[a] for a in sorted(relocs) if addr <= a < end]
        shown.update(a for a in relocs if addr <= a < end)
        if not args.plain and not tags and PADDING.search(line):
            continue

        # The AT&T spelling of a popping divide or subtract is its own
        # opposite.  Where the Intel rendering of the SAME BYTES disagrees,
        # say so on the line rather than trusting the reader to remember.
        att_mn = line.expandtabs(8).split()
        att_mn = att_mn[att_mn.index(":") + 1] if ":" in att_mn else None
        m2 = re.match(r"\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} )+\s*(\S+)", line)
        att_mn = m2.group(1) if m2 else None
        if att_mn in REVERSIBLE:
            other = intel_at.get(addr)
            if other and other != att_mn:
                tags.append("Intel: %s" % other)

        text = line.rstrip()
        if tags:
            text = "%-58s  <== %s" % (text.expandtabs(8).rstrip(),
                                      ", ".join(tags))
        out.append(text)

    print("\n".join(out))
    print("\n[%d relocation(s) in %s range, %d shown inline; "
          "%d instruction(s) decoded, %d displayed]"
          % (len(relocs), section, len(shown), len(starts), len(out)), file=sys.stderr)
    if shown != set(relocs):
        raise ValueError("relocations outside decoded instruction bytes: %s" %
                         sorted(set(relocs) - shown))


if __name__ == "__main__":
    main()
