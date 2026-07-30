#!/usr/bin/env python3
"""Find which data objects a relocation actually points at.

dsplibs.o is a partial link, so a config struct holding a pointer stores it
as an `R_386_32` against a *section* symbol with the real target as an inline
addend.  Dump such a struct as int16 and the pointer shows up as a pair of
ordinary-looking coefficients -- `AGCb103_CFG` reads 30736 and 30732 where it
means `AGC_DEF_ALPHA` and `AGC_DEF_BETA`.  That has already cost this project
two misreadings, so:

    tabdump.py    warns that a range contains relocations
    relocscan.py  resolves them to names, which is what you actually wanted

Usage:

    # what does this address get referenced by, and what is at it?
    relocscan.py <obj> --at .data:0x7810

    # which objects matching a pattern are pointed at, and at what offset
    # inside them?  The offset is the interesting part: an array selected at
    # [+0] everywhere means its later elements are dead.
    relocscan.py <obj> --into AGC_DEF

    # everything pointing into a range
    relocscan.py <obj> --range .data:0x7800-0x7820
"""

import argparse
import re
import struct
import subprocess
import sys

import elfinfo


def section_bytes(obj, name):
    """Raw contents of one section.  Empty for .bss and friends."""
    out = subprocess.run(
        ["objcopy", "-O", "binary", "--only-section=" + name, obj, "/dev/stdout"],
        capture_output=True)
    return out.stdout


def read_relocs(obj):
    """Every R_386_32 relocation, resolved to (from_sec, from_off, to_sec, to_addr).

    Only relocations against a *section* symbol carry their target in the
    addend; ones against a named symbol are already legible in `objdump -r`
    and are returned with to_addr None.
    """
    txt = subprocess.run(["objdump", "-r", obj],
                         capture_output=True, text=True).stdout
    cache = {}
    cur = None
    out = []

    for line in txt.splitlines():
        m = re.match(r"RELOCATION RECORDS FOR \[(.+)\]:", line)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r"^([0-9a-f]{8})\s+(\S+)\s+(\S+)", line)
        if not m or cur is None:
            continue
        off, rtype, sym = int(m.group(1), 16), m.group(2), m.group(3).strip()
        if rtype != "R_386_32":
            continue
        if not sym.startswith("."):
            out.append((cur, off, sym, None))
            continue
        if cur not in cache:
            cache[cur] = section_bytes(obj, cur)
        data = cache[cur]
        if off + 4 > len(data):
            continue
        out.append((cur, off, sym, struct.unpack("<I", data[off:off + 4])[0]))
    return out


def data_objects(obj, pattern=None):
    """Named OBJECT symbols, as (section, addr, size, name).

    Duplicates are kept: a file-static name appears once per translation unit
    that declared it, at a different address each time, and telling them apart
    is the whole point.
    """
    sections = elfinfo.read_sections(obj)
    found = []
    for s in elfinfo.read_symbols(obj):
        if s.type != "OBJECT" or not s.name or not s.size:
            continue
        if pattern and pattern not in s.name:
            continue
        sec = elfinfo.section_name(sections, s.ndx)
        if sec is None:
            continue
        found.append((sec, s.value, s.size, s.name))
    return sorted(found)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("obj")
    ap.add_argument("--into", metavar="PATTERN",
                    help="report references into objects whose name contains "
                         "PATTERN, with the offset reached inside each")
    ap.add_argument("--at", metavar="SECTION:ADDR",
                    help="report references to exactly this address")
    ap.add_argument("--range", metavar="SECTION:LO-HI",
                    help="report references landing in this range")
    args = ap.parse_args()

    relocs = read_relocs(args.obj)
    resolved = [r for r in relocs if r[3] is not None]
    print("# %d R_386_32 relocations, %d against a section symbol"
          % (len(relocs), len(resolved)))

    if args.into:
        objs = data_objects(args.obj, args.into)
        if not objs:
            sys.exit("error: no OBJECT symbol matches %r" % args.into)
        print("# %d objects match %r\n" % (len(objs), args.into))
        for sec, addr, size, name in objs:
            hits = {}
            for fsec, foff, tsec, taddr in resolved:
                if tsec == sec and addr <= taddr < addr + size:
                    hits[taddr - addr] = hits.get(taddr - addr, 0) + 1
            if hits:
                where = ", ".join("[+%d] x%d" % (d, n)
                                  for d, n in sorted(hits.items()))
            else:
                where = "unreferenced"
            print("  %-10s 0x%06x  %-22s size %-4d  %s"
                  % (sec, addr, name, size, where))
        return

    if args.at:
        sec, _, addr = args.at.partition(":")
        lo = hi = int(addr, 0)
    elif args.range:
        sec, _, span = args.range.partition(":")
        lostr, _, histr = span.partition("-")
        lo, hi = int(lostr, 0), int(histr, 0)
    else:
        sys.exit("error: need --into, --at or --range")

    objs = data_objects(args.obj)
    print()
    n = 0
    for fsec, foff, tsec, taddr in resolved:
        if tsec != sec or not (lo <= taddr <= hi):
            continue
        n += 1
        label = ""
        for osec, oaddr, osize, oname in objs:
            if osec == tsec and oaddr <= taddr < oaddr + osize:
                label = "  = %s+%d" % (oname, taddr - oaddr)
                break
        print("  %s+0x%06x -> %s:0x%06x%s" % (fsec, foff, tsec, taddr, label))
    if not n:
        print("  (nothing points there)")


if __name__ == "__main__":
    main()
