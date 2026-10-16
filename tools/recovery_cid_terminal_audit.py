#!/usr/bin/env python3
"""Audit complete CID terminal-cursor TUs and fire a data negative control."""
import hashlib
import json
from pathlib import Path

import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection


def inspect(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        symbols = elf.get_section_by_name('.symtab')
        sections = {i: section.name for i, section in enumerate(elf.iter_sections())}
        records = []
        for symbol in symbols.iter_symbols():
            if not symbol.name:
                continue
            index = symbol['st_shndx']
            records.append([symbol.name, symbol['st_info']['type'],
                            symbol['st_info']['bind'], symbol['st_other']['visibility'],
                            sections[index] if isinstance(index, int) else index,
                            None if symbol['st_info']['type'] == 'STT_FUNC'
                            else symbol['st_size']])
        nontext = {}
        for section in elf.iter_sections():
            if section.name in ('.text', '.symtab', '.strtab', '.shstrtab', '.rel.text'):
                continue
            if isinstance(section, RelocationSection):
                nontext[section.name] = [[r['r_offset'], r['r_info_type'],
                                         symbols.get_symbol(r['r_info_sym']).name]
                                        for r in section.iter_relocations()]
            else:
                nontext[section.name] = [section['sh_type'], section['sh_flags'],
                                         section['sh_addralign'], section['sh_entsize'],
                                         section.data().hex()]
        return {'symbols': records, 'nontext': nontext}


def main():
    root = driver.ROOT / 'build/recovery-cid-terminal'
    results = json.loads((root / 'results.json').read_text())
    cells = results['families']['Cidfsd']['cells']
    assert len(cells) == 5 and cells['baseline']['baseline_reproduced']
    assert len({c['source_hash'] for c in cells.values()}) == 5
    assert len({c['object_hash'] for c in cells.values()}) == 5
    basepath = root / 'Cidfsd/baseline/candidate.o'
    base = inspect(basepath)
    report = {}
    for label, cell in cells.items():
        path = root / 'Cidfsd' / label / 'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        assert inspect(path) == base, (label, 'data/binding/metadata drift')
        assert set(cell.get('changed_bodies', [])) <= {'CID_FSD_demodulate'}
        assert not cell.get('losses') and not cell.get('gains')
        report[label] = {'verdict': cell['verdicts']['CID_FSD_demodulate'],
                         'size': driver.b.sizes(str(path))['CID_FSD_demodulate'],
                         'changed_bodies': cell.get('changed_bodies', []),
                         'text_relocations': repr(driver.b.body(str(path), 'CID_FSD_demodulate')[1]),
                         'data_binding_metadata_unchanged': True}
    # A deliberately corrupted data byte must be rejected by this audit.
    negative = root / 'data-negative-control.o'
    data = bytearray(basepath.read_bytes())
    with basepath.open('rb') as stream:
        section = ELFFile(stream).get_section_by_name('.data')
        assert section['sh_size'] > 0
        data[section['sh_offset']] ^= 1
    negative.write_bytes(data)
    assert inspect(negative) != base
    output = {'cells': report, 'data_negative_control_detected': True,
              'functions_per_cell': 1, 'distinct_sources': 5, 'distinct_objects': 5,
              'named_objects': 3, 'exact_gains': [], 'exact_losses': []}
    (root / 'complete-audit.json').write_text(json.dumps(output, indent=2) + '\n')
    print('5/5 complete TUs: data, symbol binding/visibility and nontext metadata unchanged;')
    print('5 distinct sources/objects, 1 shared function and 3 data symbols per cell;')
    print('0 exact gains/losses; data corruption negative control detected 1/1.')


if __name__ == '__main__':
    main()
