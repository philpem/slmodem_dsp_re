#!/usr/bin/env python3
"""
Extract a named data table out of dsplibs.o and emit it as C.

Coefficient and lookup tables are a large part of what was lost with the
source, and they are the part a human cannot simply re-derive by reading
disassembly. This pulls the raw bytes out so they can be compared against a
*generator* - the reconstruction's job is to produce a function that recreates
the table from its design parameters, and `make test` checks the generator
reproduces these bytes exactly.

Tables reached only through a config struct have no relocation pointing at
them by name, so `--at SECTION:OFFSET` handles the anonymous case.

Element types follow the DSP code's own conventions:
    u8 s8 u16 s16 u32 s32 f32 f64

Usage:
    tabdump.py <obj> --sym seg_end --type s16 --count 8
    tabdump.py <obj> --at .data:0x9480 --type s16 --count 8 --name seg_end
    tabdump.py <obj> --sym _a2u --type u8            # length from st_size
"""

import argparse
import re
import struct
import subprocess
import sys

import elfinfo

FORMATS = {
    "u8": ("B", 1), "s8": ("b", 1),
    "u16": ("<H", 2), "s16": ("<h", 2),
    "u32": ("<I", 4), "s32": ("<i", 4),
    "f32": ("<f", 4), "f64": ("<d", 8),
}

CTYPE = {
    "u8": "unsigned char", "s8": "signed char",
    "u16": "unsigned short", "s16": "short",
    "u32": "unsigned int", "s32": "int",
    "f32": "float", "f64": "double",
}


def section_bytes(obj, name):
    """Raw contents of one section, via objcopy (works on .o without loading)."""
    out = subprocess.run(
        ["objcopy", "-O", "binary", "--only-section", name, obj, "/dev/stdout"],
        capture_output=True, check=True).stdout
    return out


def relocations_in(obj, section, lo, hi):
    """Relocation offsets within [lo, hi) of `section`.

    Load-bearing.  A table that looks like plain numbers may contain embedded
    pointers, and in a .o those read as the *addend* only -- the linker
    supplies the rest.  FPM_TONE_CFG is exactly this: dumped as int16 its
    +0x10 field reads -12224, which looks like a scalar and is in fact the low
    half of a pointer to a waveform table.  Interpreting it as data sends the
    reader down a blind alley.
    """
    out = subprocess.run(["readelf", "-rW", obj],
                         capture_output=True, text=True).stdout
    hits, cur = [], None
    for line in out.splitlines():
        m = re.search(r"Relocation section '(\S+)'", line)
        if m:
            cur = m.group(1)
            continue
        if cur != ".rel" + section:
            continue
        m = re.match(r"^([0-9a-f]{8})\s+\S+\s+(\S+)\s+\S*\s*(.*)$",
                     line.strip())
        if m and lo <= int(m.group(1), 16) < hi:
            hits.append((int(m.group(1), 16), m.group(2), m.group(3).strip()))
    return hits


def find_symbol(obj, sym):
    """Resolve a symbol name, refusing to guess between duplicates.

    dsplibs.o is a partial link of 283 translation units, so a name that was
    file-static in the original appears once per TU that declared it -- there
    are eight distinct AGC_DEF_ALPHA objects, at eight different addresses,
    holding different values.  Silently taking the first match dumps some
    other datapump's coefficients under the name you asked for, and they look
    entirely plausible.  That happened; hence this check.
    """
    matches = [s for s in elfinfo.read_symbols(obj) if s.name == sym]
    if not matches:
        return None, None, None

    sections = elfinfo.read_sections(obj)
    distinct = sorted({(s.ndx, s.value, s.size) for s in matches})
    if len(distinct) > 1:
        where = "\n".join(
            "    --at %s:0x%x --count N   (st_size %d)"
            % (elfinfo.section_name(sections, ndx), value, size)
            for ndx, value, size in distinct)
        sys.exit(
            "error: %d distinct symbols are named %r -- this name was "
            "file-static in\nthe original, so each translation unit that "
            "declared it has its own.\nPick one explicitly:\n\n%s\n"
            % (len(distinct), sym, where))

    s = matches[0]
    return elfinfo.section_name(sections, s.ndx), s.value, s.size


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("obj")
    ap.add_argument("--sym")
    ap.add_argument("--at", help="SECTION:OFFSET for an unnamed table")
    ap.add_argument("--type", default="s16", choices=sorted(FORMATS))
    ap.add_argument("--count", type=int)
    ap.add_argument("--name")
    ap.add_argument("--per-line", type=int, default=8)
    args = ap.parse_args()

    if args.sym:
        sec, off, size = find_symbol(args.obj, args.sym)
        if sec is None:
            sys.exit("error: symbol %r not found in %s" % (args.sym, args.obj))
        name = args.name or args.sym
    elif args.at:
        sec, _, off = args.at.partition(":")
        off, size = int(off, 0), 0
        name = args.name or ("tab_%s_%x" % (sec.lstrip(".").replace(".", "_"), off))
    else:
        sys.exit("error: need --sym or --at")

    fmt, width = FORMATS[args.type]
    count = args.count or (size // width if size else 0)
    if not count:
        sys.exit("error: symbol has no st_size; pass --count")

    blob = section_bytes(args.obj, sec)
    end = off + count * width
    if end > len(blob):
        sys.exit("error: %s:0x%x+%d runs past end of %s (%d bytes)"
                 % (sec, off, count * width, sec, len(blob)))

    vals = [struct.unpack_from(fmt, blob, off + i * width)[0]
            for i in range(count)]

    # Warn loudly: an embedded pointer is not data, and reading it as such is
    # a silent misinterpretation rather than an error.
    relocs = relocations_in(args.obj, sec, off, end)
    if relocs:
        print("/*\n * WARNING: %d relocation(s) inside this range -- the values"
              " below are\n * ADDENDS, not final values.  These offsets hold"
              " pointers or\n * section-relative references, not data:\n *"
              % len(relocs))
        for r_off, r_type, r_sym in relocs:
            print(" *   +0x%04x  %-14s %s" % (r_off - off, r_type, r_sym))
        print(" */")

    print("/* %s[%d] - extracted from %s %s:0x%06x by tools/tabdump.py.\n"
          " * Reference bytes only: the maintainable form is a generator that\n"
          " * reproduces these values from the table's design parameters. */"
          % (name, count, args.obj, sec, off))
    print("static const %s %s[%d] = {" % (CTYPE[args.type], name, count))
    for i in range(0, count, args.per_line):
        row = vals[i:i + args.per_line]
        if args.type in ("f32", "f64"):
            body = ", ".join("%.9gf" % v if args.type == "f32" else "%.17g" % v
                             for v in row)
        else:
            body = ", ".join("%d" % v for v in row)
        print("\t%s," % body)
    print("};")


if __name__ == "__main__":
    main()
