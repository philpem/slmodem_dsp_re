#!/usr/bin/env python3
"""Every .text symbol reached only through a POINTER stored in data.

closure.py walks calls and data-to-data references, but its .rel.data pass
adds D and R symbols only -- a function pointer parked in a dispatch table
or a C++ vtable is never followed.  Those pointers are R_386_32 against a
SECTION symbol with the addend inline in the section contents, so objdump
prints ".text" and names nothing.

This reads every relocation in every data-ish section, resolves the inline
addend back to a .text symbol, and prints the ones that land exactly on a
symbol boundary.  Landing mid-function means a switch jump table, which is
intra-function control flow and not an entry point; those are counted
separately rather than dropped silently.
"""
import os
import subprocess
import sys
from collections import defaultdict

#
# `tools/dis.py` shadows the standard library's `dis`, and `inspect` imports
# that on the way up from pyelftools -- so this file died on the import below
# with
#
#     AttributeError: module 'dis' has no attribute 'COMPILER_FLAG_NAMES'
#
# naming neither this directory nor the file responsible, for as long as the
# tool had existed.  Drop our own directory from the search path before the
# import rather than renaming dis.py, whose name is right and which is
# referenced from several findings.  Same fix as whichfield.py and
# boundarycheck.py; finding F3520.
#
_here = os.path.abspath(os.path.dirname(__file__))
sys.path[:] = [p for p in sys.path if os.path.abspath(p or ".") != _here]

from elftools.elf.elffile import ELFFile                    # noqa: E402
from elftools.elf.sections import SymbolTableSection        # noqa: E402


def default_blob():
    """$BLOB, else the main repository's copy -- the Makefile's own rule.

    A plain `ref/slmodemd/dsplibs.o` is wrong in every agent worktree, since
    those live under `.claude/worktrees/` and it resolves to
    `.claude/worktrees/slmodemd`.  `git rev-parse --git-common-dir` names the
    MAIN repository's .git from inside any worktree, which is how `BLOB` in
    the Makefile and the `prereq` target both find their way out.
    """
    if os.environ.get("BLOB"):
        return os.environ["BLOB"]
    try:
        gcd = subprocess.check_output(["git", "rev-parse", "--git-common-dir"],
                                      text=True).strip()
    except Exception:
        return "ref/slmodemd/dsplibs.o"
    return os.path.abspath(os.path.join(gcd, os.pardir, os.pardir,
                                        "slmodemd", "dsplibs.o"))


# The resolved path is PRINTED with the counts below: a tool that can silently
# read a different object than the caller meant is exactly the hazard
# `readyqueue.py`'s comment is about, and printing it costs one line.
path = sys.argv[1] if len(sys.argv) > 1 else default_blob()

with open(path, "rb") as fh:
    elf = ELFFile(fh)
    sections = list(elf.iter_sections())
    name_to_idx = {s.name: i for i, s in enumerate(sections)}
    text_idx = name_to_idx[".text"]

    symtab = next(s for s in sections if isinstance(s, SymbolTableSection))

    text_at = {}
    for sym in symtab.iter_symbols():
        if sym["st_shndx"] == text_idx and sym.name:
            text_at.setdefault(sym["st_value"], sym.name)

    contents = {}
    for i, s in enumerate(sections):
        if s.header["sh_type"] != "SHT_NOBITS":
            try:
                contents[i] = s.data()
            except Exception:
                pass

    exact = defaultdict(set)   # target symbol -> sections it is pointed from
    onsym = 0                  # RELOCATIONS landing on a boundary, not symbols
    midfn = 0
    total = 0

    for s in sections:
        if not s.name.startswith(".rel"):
            continue
        target_name = s.name[4:]
        if target_name == ".text" or target_name not in name_to_idx:
            continue
        ti = name_to_idx[target_name]
        if ti not in contents:
            continue
        blob = contents[ti]
        for reloc in s.iter_relocations():
            if reloc["r_info_type"] != 1:      # R_386_32 only
                continue
            sym = symtab.get_symbol(reloc["r_info_sym"])
            if sym["st_shndx"] != text_idx:
                continue
            off = reloc["r_offset"]
            addend = int.from_bytes(blob[off:off + 4], "little")
            # a named symbol carries its own value; a section symbol does not
            addr = addend + (sym["st_value"] if sym.name else 0)
            total += 1
            if addr in text_at:
                exact[text_at[addr]].add(target_name)
                onsym += 1
            else:
                midfn += 1

print("pointers stored in data that resolve to a .text symbol")
print()
print("  object: %s" % os.path.abspath(path))
print("  %d relocations into .text from data sections" % total)
# The two numbers on this line are different measurements and the arithmetic
# only closes on the first: one symbol can be pointed at from many tables.
print("  %d land exactly on a symbol, naming %d distinct entry points"
      % (onsym, len(exact)))
print("  %d land mid-function (switch jump tables, not entry points)" % midfn)
print()
for name in sorted(exact):
    print("%s\t%s" % (name, ",".join(sorted(exact[name]))))
