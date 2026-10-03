#!/usr/bin/env python3
"""Audit all four issue246 full-TU cells, including data and bystanders."""
import json
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

b = driver.b
# Reuse the existing bounded ELF metadata/data inspector.
source = (driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])
root = driver.ROOT/'build/gcc3-alpha-energy/V34TX'
reports = {}
for case in ('baseline', 'short-quotient', 'energy-update', 'energy-update-short'):
    path = root/case/'candidate.o'
    report = inspect(path)
    report.update(nontext={}, nontext_relocations={})
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        syms = elf.get_section_by_name('.symtab')
        names = {i:s.name for i,s in enumerate(elf.iter_sections())}
        for index, section in enumerate(elf.iter_sections()):
            if not section['sh_flags'] & 2 or section.name == '.text':
                continue
            raw, canonical = bytearray(section.data()), {}
            for relsec in elf.iter_sections():
                if not isinstance(relsec, RelocationSection) or relsec['sh_info'] != index:
                    continue
                for relocation in relsec.iter_relocations():
                    offset = relocation['r_offset']
                    symbol = syms.get_symbol(relocation['r_info_sym'])
                    kind = {1:'R_386_32', 2:'R_386_PC32'}[relocation['r_info_type']]
                    canonical[offset] = (kind, b.relocation_target(kind, symbol.name or names[symbol['st_shndx']], bytes(raw[offset:offset+4]), b.section_symbols(str(path))))
                    raw[offset:offset+4] = b'\0'*4
            report['nontext'][section.name] = raw.hex()
            report['nontext_relocations'][section.name] = canonical
    reports[case] = report
    for key in ('records', 'objects', 'allocated_sizes', 'nontext', 'nontext_relocations'):
        assert report[key] == reports['baseline'][key], (case, key)

base = str(root/'baseline/candidate.o')
candidate = str(root/'energy-update-short/candidate.o')
functions = sorted(b.sizes(base))
changed = [name for name in functions if b.body(base,name) != b.body(candidate,name)]
assert len(functions) == 7 and len(reports['baseline']['objects']) == 0
assert changed == ['adaptecho', 'txinit', 'updateAlpha'], changed
assert b.alpha_why(b.insns(base,'txinit'), b.insns(candidate,'txinit')) is None
assert b.verdict(*b.body(b.BLOB,'updateAlpha'), *b.body(candidate,'updateAlpha')) == ('EXACT', 0)
reports['scope'] = {'cells':4, 'functions':7, 'named_data':0, 'changed':changed,
                    'txinit_alpha_equal':True, 'updateAlpha_exact':True}
(root.parent/'complete-object-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('4/4 cells: 7 functions, 0 named data; metadata/nontext/relocation controls pass')
print('updateAlpha EXACT; txinit register-only change; changed bodies:', changed)
