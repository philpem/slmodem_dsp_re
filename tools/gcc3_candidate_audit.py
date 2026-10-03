#!/usr/bin/env python3
"""Audit every declared issue248 full-TU cell, including canonical data relocations."""
import json
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

b = driver.b
# Reuse the existing bounded ELF metadata/data inspector.
source = (driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])

roots = sorted(driver.ROOT.glob('build/gcc3-screen-*/results.json'))
reports_by_family = {}
count = 0
for ledger in roots:
    if 'invalid' in str(ledger):
        continue
    roots_in_family = sorted(ledger.parent.glob('*/baseline/candidate.o'))
    for baseline in roots_in_family:
        root = baseline.parent.parent
        cases = ['baseline'] + sorted(p.parent.name for p in root.glob('*/candidate.o') if p.parent.name != 'baseline')
        reports = {}
        for case in cases:
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
                            target = canonical[offset][1]
                            if target[:2] == ('section', '.text'):
                                address = target[2]
                                owners = [sym for sym in syms.iter_symbols()
                                          if sym['st_info']['type'] == 'STT_FUNC'
                                          and sym['st_shndx'] == elf.get_section_index('.text')
                                          and sym['st_value'] <= address < sym['st_value'] + sym['st_size']]
                                assert len(owners) == 1, (path, address, owners)
                                owner = owners[0]
                                canonical[offset] = (kind, ('function', owner.name, address-owner['st_value']))
                            raw[offset:offset+4] = b'\0'*4
                    report['nontext'][section.name] = raw.hex()
                    report['nontext_relocations'][section.name] = canonical
            reports[case] = report
            for key in ('records', 'objects', 'allocated_sizes', 'nontext', 'nontext_relocations'):
                assert report[key] == reports['baseline'][key], (str(root), case, key)

        reports_by_family[str(root.relative_to(driver.ROOT))] = reports
        count += len(cases)
        print(root.name, ledger.parent.name, len(cases), 'cells', len(b.sizes(str(baseline))), 'functions', len(reports['baseline']['objects']), 'named data: controls pass')
assert count == 27, count
(driver.ROOT/'build/gcc3-candidate-full-tu-audit.json').write_text(json.dumps(reports_by_family, indent=2)+'\n')
print('27/27 valid cells: metadata, named data, allocated nontext and canonical relocation controls pass')
