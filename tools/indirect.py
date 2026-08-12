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
import sys
from collections import defaultdict
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

path = sys.argv[1]

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
            else:
                midfn += 1

print("pointers stored in data that resolve to a .text symbol")
print()
print("  %d relocations into .text from data sections" % total)
print("  %d land exactly on a symbol (indirect entry points)" % len(exact))
print("  %d land mid-function (switch jump tables, not entry points)" % midfn)
print()
for name in sorted(exact):
    print("%s\t%s" % (name, ",".join(sorted(exact[name]))))
