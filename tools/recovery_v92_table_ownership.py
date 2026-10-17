#!/usr/bin/env python3
"""Cross encoder static-definition order with unit-at-a-time emission.

Four complete-TU controls distinguish table layout from code and call-boundary
fidelity. Relative layout does not uniquely recover original declarations.
The subset-first/unit cell is retained as a bounded original data-layout
recovery under unchanged flags; the original source order remains uncertain.

  mkdir -p build
  python3 tools/recovery_v92_table_ownership.py --domain definition-order \\
      --baseline-dir build/archived-v92-baseline
  python3 tools/recovery_v92_table_ownership.py --audit

The production-control mode checks the historical table-only source. After
case-boundary adoption use recovery_v92_boundary_adoption.py's production
control for the combined current source.
"""
import re
import json
import sys
import subprocess
from pathlib import Path
import playbook_small_patterns as d

d.REV = '8e216110a'
d.OUT_NAME = 'v92-table-ownership'
d.SOURCE_PATHS = ('src/pump/v90/V92ConvEncoder.cpp',)
d.DUMP_FLAGS = ('-da', '-v', '-save-temps', '-fdump-tree-original',
                '-fdump-tree-inlined', '-fdump-tree-optimized')


def swap_definitions(source):
    pattern = (r'(int V92ConvolutionEncoder::cosetMapping4D\[64\] = \{.*?\n\};)\n\n'
               r'(int V92ConvolutionEncoder::subsetLabelTable\[16\] = \{.*?\n\};)')
    swapped, count = re.subn(pattern, lambda m: m[2] + '\n\n' + m[1], source, flags=re.S)
    assert count == 1 and swapped != source
    return swapped


def variants(path, source):
    swapped = swap_definitions(source)
    return {'baseline': source,
            'coset-first-no-unit': source,
            'subset-first-unit': swapped,
            'subset-first-no-unit': swapped}


d.variants = variants
d.CELL_FLAGS = lambda path, label: ('-fno-unit-at-a-time',) if label.endswith('-no-unit') else ()

PREFIX = '_ZN21V92ConvolutionEncoder'
COSET, SUBSET = PREFIX + '14cosetMapping4DE', PREFIX + '16subsetLabelTableE'


def snapshot(path):
    from elftools.elf.elffile import ELFFile
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        tables = {}
        for symbol in table.iter_symbols():
            if symbol.name in (COSET, SUBSET):
                section = elf.get_section(symbol['st_shndx'])
                at, size = symbol['st_value'], symbol['st_size']
                tables[symbol.name] = dict(section=section.name, offset=at, size=size,
                                           type=symbol['st_info']['type'], bind=symbol['st_info']['bind'],
                                           payload=section.data()[at:at + size].hex())
        assert set(tables) == {COSET, SUBSET}
        raw_relocs, references = [], []
        functions = [s for s in table.iter_symbols() if s['st_info']['type'] == 'STT_FUNC' and s['st_size']]
        for section in elf.iter_sections():
            if section['sh_type'] != 'SHT_REL':
                continue
            owner = elf.get_section(section['sh_info'])
            symbols = elf.get_section(section['sh_link'])
            for relocation in section.iter_relocations():
                at, kind = relocation['r_offset'], relocation['r_info_type']
                symbol = symbols.get_symbol(relocation['r_info_sym'])
                raw = owner.data()[at:at + 4]
                addend = int.from_bytes(raw, 'little', signed=True)
                raw_relocs.append([owner.name, at, kind, symbol.name, addend])
                if kind != 1 or not isinstance(symbol['st_shndx'], int):
                    continue
                target_section = elf.get_section(symbol['st_shndx'])
                address = (symbol['st_value'] + int.from_bytes(raw, 'little')) & 0xffffffff
                targets = [name for name, data in tables.items() if target_section.name == data['section'] and
                           data['offset'] <= address < data['offset'] + data['size']]
                if targets:
                    callers = [s.name for s in functions if s['st_shndx'] == section['sh_info'] and
                               s['st_value'] <= at < s['st_value'] + s['st_size']]
                    references.append(dict(source_section=owner.name, offset=at, functions=callers,
                                           symbol=symbol.name, addend=addend, targets=targets))
        data = elf.get_section_by_name('.data')
        return dict(tables=tables, data=data.data(), data_file_offset=data['sh_offset'],
                    raw_relocations=raw_relocs, table_references=references)


def audit():
    from gcc3_value_carriers_audit import inspect
    import local_data_ownership
    root = d.ROOT / 'build' / d.OUT_NAME
    folder = root / 'V92ConvEncoder'
    result = json.loads((root / 'results.json').read_text())
    cells = result['families']['V92ConvEncoder']['cells']
    expected = {'baseline': False, 'coset-first-no-unit': True,
                'subset-first-unit': True, 'subset-first-no-unit': False}
    assert set(cells) == set(expected)
    assert cells['baseline']['baseline_reproduced']
    assert result['revision'] == d.REV and 'DSPLIB_REPRODUCE_BUGS' in result['config']
    original = snapshot(Path(d.b.BLOB))
    baseline = snapshot(folder / 'baseline/candidate.o')
    metadata = inspect(folder / 'baseline/candidate.o')
    block = original['data'][0x6ac0:0x6c00]
    assert len(block) == len(baseline['data']) == 320
    report = {'cells': {}, 'original_tables': original['tables'],
              'original_table_references': original['table_references']}
    for label, original_order in expected.items():
        path = folder / label / 'candidate.o'
        observed, current, cell = inspect(path), snapshot(path), cells[label]
        assert len(cell['functions']) == len(cell['verdicts']) == 8
        assert cell.get('changed_bodies', []) == []
        assert cell.get('gains', []) == cell.get('losses', []) == []
        assert sum(v[0] == 'EXACT' for v in cell['verdicts'].values()) == 6
        for key in ('text_positions', 'nobits', 'relocations'):
            assert observed[key] == metadata[key], (label, key)
        records = {name: values[:] for name, values in metadata['records'].items()}
        records[COSET][-2] = 0 if original_order else 64
        records[SUBSET][-2] = 256 if original_order else 0
        assert observed['records'] == records, label
        allocated = dict(metadata['allocated'], **{'.data': (block if original_order else baseline['data']).hex()})
        assert observed['allocated'] == allocated, label
        assert current['raw_relocations'] == baseline['raw_relocations'], label
        assert current['data'] == (block if original_order else baseline['data'])
        for name in (COSET, SUBSET):
            assert current['tables'][name]['payload'] == original['tables'][name]['payload']
            assert current['tables'][name]['bind'] == 'STB_GLOBAL'
            assert current['tables'][name]['type'] == 'STT_OBJECT'
        report['cells'][label] = dict(original_block_matches=original_order, tables=current['tables'],
                                     changed_bodies=[], gains=[], losses=[],
                                     object_sha256=cell['object_hash'], changed_raw_relocations=[])
    assert (folder / 'coset-first-no-unit/candidate.o').read_bytes() == (folder / 'subset-first-unit/candidate.o').read_bytes()
    assert (folder / 'baseline/candidate.o').read_bytes() == (folder / 'subset-first-no-unit/candidate.o').read_bytes()
    # Exercise the complete-block detector with a real ELF data-byte change.
    selected = folder / 'subset-first-unit/candidate.o'
    bad = bytearray(selected.read_bytes())
    bad[snapshot(selected)['data_file_offset']] ^= 1
    negative = root / 'changed-byte-negative.o'
    negative.write_bytes(bad)
    assert snapshot(negative)['data'] != block
    ownership = local_data_ownership.build(d.b.BLOB)
    rows = [row for row in ownership['records'].values() if row['function']['name'].startswith(PREFIX)]
    assert len(rows) == 8 and all(row['status'] == 'no-witness' for row in rows)
    helper = next(row for row in rows if row['function']['name'] == PREFIX + '10inverseMapEPi')
    assert helper['rejections']['GLOBAL-or-WEAK-data-does-not-establish-owner'] == 3
    refs = original['table_references']
    assert len(refs) == 3 and all(ref['functions'] == [PREFIX + '10inverseMapEPi'] for ref in refs)
    assert all(ref['symbol'] in (COSET, SUBSET) and ref['addend'] == 0 for ref in refs)
    report['ownership'] = rows
    report['changed_byte_negative_control'] = 'detected'
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('V92 table ownership: 4 cells / 32 body verdicts, all 8 bodies/positions and 6 exact names preserved; '
          '2 raw-object equivalence pairs; selected complete 320-byte block matches original; '
          '0/8 LOCAL-owner witnesses, 3 GLOBAL table references; changed-byte control fired')


def production_control():
    root = d.ROOT / 'build' / d.OUT_NAME
    result = json.loads((root / 'results.json').read_text())
    cell = root / 'production-control'
    cell.mkdir(exist_ok=True)
    source = d.ROOT / d.SOURCE_PATHS[0]
    (cell / source.name).write_bytes(source.read_bytes())
    command = result['families']['V92ConvEncoder']['cells']['baseline']['command']
    command = [value.replace('/V92ConvEncoder/baseline', '/production-control') for value in command]
    command[-1] = command[-1].replace('cd /work/production-control && ', 'cd /work/production-control || exit 1; ')
    (cell / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (cell / 'compile.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    assert (cell / 'candidate.o').read_bytes() == (root / 'V92ConvEncoder/subset-first-unit/candidate.o').read_bytes()
    print('Production source/comment: raw complete object matches selected subset-first/unit cell')

if __name__ == '__main__':
    if sys.argv[1:] == ['--audit']:
        audit()
    elif sys.argv[1:] == ['--production-control']:
        production_control()
    else:
        d.main()
