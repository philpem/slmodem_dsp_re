#!/usr/bin/env python3
"""Audit the bounded five-way MakeTxData table; does not change byteident grades."""
import json
from pathlib import Path
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

b = driver.b
OUT = driver.ROOT / 'build/playbook-v22-txdata-controls'
NAME = 'MakeTxData'


def table(path):
    raw, relocs = b.body(str(path), NAME)
    assert len(relocs) == 1
    off, (kind, target) = next(iter(relocs.items()))
    assert kind == 'R_386_32' and target[:2] == ('section', '.rodata')
    # This is a named five-selector source-domain audit, not generic dispatch
    # inference. The full-body comparison retains the actual guard and index.
    start = target[2]
    with open(path, 'rb') as stream:
        elf = ELFFile(stream)
        symtab = elf.get_section_by_name('.symtab')
        owners = [s for s in symtab.iter_symbols() if s.name == NAME]
        assert len(owners) == 1
        owner = owners[0]
        section_id = next(i for i, s in enumerate(elf.iter_sections())
                          if s.name == '.rodata')
        data = elf.get_section(section_id).data()[start:start + 20]
        assert len(data) == 20
        entries = [r for s in elf.iter_sections()
                   if isinstance(s, RelocationSection) and s['sh_info'] == section_id
                   for r in s.iter_relocations() if start <= r['r_offset'] < start + 20]
        assert len(entries) == 5
        assert sorted(r['r_offset'] - start for r in entries) == [0, 4, 8, 12, 16]
        targets = []
        for r in sorted(entries, key=lambda r: r['r_offset']):
            symbol = symtab.get_symbol(r['r_info_sym'])
            assert r['r_info_type'] == 1
            assert symbol['st_info']['type'] == 'STT_SECTION'
            assert symbol['st_shndx'] == owner['st_shndx']
            pos = r['r_offset'] - start
            address = int.from_bytes(data[pos:pos + 4], 'little')
            interiors = [s for s in symtab.iter_symbols()
                         if s['st_info']['type'] == 'STT_FUNC'
                         and s['st_shndx'] == owner['st_shndx']
                         and s['st_value'] <= address < s['st_value'] + s['st_size']]
            assert len(interiors) == 1 and interiors[0].name == NAME
            targets.append(address - owner['st_value'])
    masked = raw[:off] + b'\0' * 4 + raw[off + 4:]
    return {'bytes': len(raw), 'relocation_offset': off, 'kind': kind,
            'table_section_offset': start, 'table_extent': 20,
            'function_relative_targets': targets, 'masked_body_hex': masked.hex()}


def main():
    reference = table(b.BLOB)
    assert reference['bytes'] == 195
    assert reference['function_relative_targets'] == [65, 103, 131, 163, 28]
    cells = json.loads((OUT / 'results.json').read_text())['families']['v22prc']['cells']
    results = {'reference': reference, 'cells': {}}
    keys = ('bytes', 'relocation_offset', 'kind', 'table_extent',
            'function_relative_targets', 'masked_body_hex')
    for label in cells:
        path = OUT / 'v22prc' / label / 'candidate.o'
        record = table(path)
        match = all(record[k] == reference[k] for k in keys)
        record['bounded_body_and_table_match'] = match
        record['strict_comparator_verdict'] = b.verdict(
            *b.body(b.BLOB, NAME), *b.body(str(path), NAME))
        results['cells'][label] = record
        print(label, record['bytes'], 'bounded body/table match', match,
              'strict', record['strict_comparator_verdict'])
    assert not results['cells']['baseline']['bounded_body_and_table_match']
    assert {label for label, record in results['cells'].items()
            if record['bounded_body_and_table_match']} == {'both'}
    results['scope'] = 'Named five-way source domain; no automatic grade/census change'
    (OUT / 'MakeTxData-table-body-proof.json').write_text(json.dumps(results, indent=2) + '\n')
    print('4 cells checked: 1 full bounded match, 3 known misses; strict grade unchanged')


if __name__ == '__main__':
    main()
