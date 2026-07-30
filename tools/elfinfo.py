"""
Shared ELF parsing helpers for the dsplibs.o reconstruction toolchain.

dsplibs.o is an `ld -r` partial link of 283 translation units built with
GCC 3.4.2.  Crucially it is *not stripped*, so the symbol table still carries
one STT_FILE entry per original source file, in link order.  Everything in
this module exists to exploit that fact.

Symbol table layout produced by `ld -r` (and relied upon here):

    FILE  "b103.c"       <- marks the start of a translation unit
    LOCAL  b103_create        }  locals belonging to b103.c, in that TU's
    LOCAL  b103_process       }  section, at addresses inside its chunk
    FILE  "dp_param.c"    <- next translation unit
    ...
    GLOBAL <all globals>  <- emitted last, NOT grouped under any FILE

So local symbols anchor a TU to an address range; globals must be attributed
by address instead.  See tumap.py for the reconstruction of full TU extents.
"""

import re
import subprocess
from collections import namedtuple

# One row of `readelf -sW` output.
Sym = namedtuple("Sym", "num value size type bind ndx name")

# One section header row of `readelf -SW` output.
Section = namedtuple("Section", "idx name type addr off size flags")

_SYM_RE = re.compile(
    r"\s*(\d+):\s+([0-9a-f]+)\s+(\d+)\s+(\w+)\s+(\w+)\s+\w+\s+(\S+)\s+(.*)$"
)
_SEC_RE = re.compile(
    r"\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)"
)


def _run(args):
    out = subprocess.run(args, capture_output=True, text=True, check=True)
    return out.stdout


def read_sections(obj):
    """Return {index: Section} for every section header in `obj`."""
    sections = {}
    for line in _run(["readelf", "-SW", obj]).splitlines():
        m = _SEC_RE.match(line)
        if not m:
            continue
        idx, name, typ, addr, off, size = m.groups()
        sections[idx] = Section(idx, name, typ, int(addr, 16),
                                int(off, 16), int(size, 16), "")
    return sections


def read_symbols(obj):
    """Return the symbol table as a list of Sym, in symtab index order.

    Order matters: FILE symbols only delimit translation units because the
    linker emits each TU's locals immediately after its FILE entry.
    """
    syms = []
    for line in _run(["readelf", "-sW", obj]).splitlines():
        m = _SYM_RE.match(line)
        if not m:
            continue
        num, value, size, typ, bind, ndx, name = m.groups()
        syms.append(Sym(int(num), int(value, 16), int(size), typ, bind,
                        ndx, name.strip()))
    return syms


def section_name(sections, ndx):
    """Map a symbol's st_shndx to a section name, or '' for ABS/UND/COMMON."""
    return sections[ndx].name if ndx.isdigit() and ndx in sections else ""


def is_source_file(name):
    """True for real translation units, excluding GCC's synthetic entries.

    GCC 3.4.2 emits `<built-in>` and `<command line>` as FILE symbols; they
    carry no code and must not be counted as translation units.
    """
    return bool(name) and not name.startswith("<")
