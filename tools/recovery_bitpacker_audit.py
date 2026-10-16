#!/usr/bin/env python3
"""Audit every CP/MP cell, retaining shifted switch-target relocation evidence."""
import argparse
from collections import Counter
import hashlib
import json
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection


def nontext_relocations(path):
    result = []
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        symtab = elf.get_section_by_name('.symtab')
        for section in elf.iter_sections():
            if not isinstance(section, RelocationSection):
                continue
            destination = elf.get_section(section['sh_info'])
            if not destination['sh_flags'] & 2 or destination.name == '.text':
                continue
            for relocation in section.iter_relocations():
                symbol = symtab.get_symbol(relocation['r_info_sym'])
                index = symbol['st_shndx']
                target_section = elf.get_section(index).name if isinstance(index, int) else index
                offset = relocation['r_offset']
                addend = int.from_bytes(destination.data()[offset:offset + 4], 'little')
                owner = [entry.name for entry in symtab.iter_symbols()
                         if entry['st_info']['type'] == 'STT_FUNC'
                         and entry['st_shndx'] == index
                         and entry['st_value'] <= addend < entry['st_value'] + entry['st_size']]
                owner_offsets = {entry.name: addend - entry['st_value']
                                 for entry in symtab.iter_symbols()
                                 if entry.name in owner}
                result.append({'section': destination.name, 'offset': offset,
                               'type': relocation['r_info_type'], 'target': symbol.name,
                               'target_section': target_section, 'addend': addend,
                               'function_owner': owner, 'owner_offsets': owner_offsets})
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run', default='recovery-bitpacker-topology')
    args = ap.parse_args()
    root = d.ROOT / 'build' / args.run
    ledger = json.loads((root / 'results.json').read_text())
    report, cells, bodies = {}, 0, 0
    for family, record in ledger['families'].items():
        baseline = root / family / 'baseline/candidate.o'
        metadata, data = inspect(baseline), allocated(baseline)
        relocation_baseline = nontext_relocations(baseline)
        rows = {}
        for label, cell in record['cells'].items():
            path = root / family / label / 'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            current = inspect(path)
            assert current['records'] == metadata['records']
            assert current['objects'] == metadata['objects']
            assert cell['globals'] == record['cells']['baseline']['globals']
            changed_pools = {}
            candidate_data = allocated(path)
            assert set(candidate_data) == set(data)
            for name, contents in candidate_data.items():
                if contents == data[name]:
                    continue
                assert name.startswith('.rodata.str'), (family, label, name)
                literals = lambda payload: Counter(x for x in bytes.fromhex(payload).split(b'\0') if x)
                assert literals(contents) == literals(data[name])
                changed_pools[name] = {'baseline': data[name], 'candidate': contents}
            relocs = nontext_relocations(path)
            assert len(relocs) == len(relocation_baseline)
            changed_targets = []
            for before, after in zip(relocation_baseline, relocs):
                for key in ('section', 'offset', 'type', 'target', 'target_section', 'function_owner'):
                    assert before[key] == after[key], (family, label, key, before, after)
                if before['addend'] != after['addend']:
                    assert before['type'] == 1 and before['target_section'] == '.text'
                    if before['owner_offsets'] != after['owner_offsets']:
                        assert before['function_owner'] in [['_ZN5V90CP10infoToBitsEv'], ['_ZN5V90MP10bitsToInfoEi']]
                    changed_targets.append({'baseline': before, 'candidate': after})
            grades = {name: list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(path), name))) for name in cell['verdicts']}
            assert grades == cell['verdicts']
            rows[label] = {'grades': grades, 'object_sha256': cell['object_hash'],
                           'exact_gains': cell.get('gains', []), 'exact_losses': cell.get('losses', []),
                           'changed_bodies': cell.get('changed_bodies', []),
                           'symbol_metadata_named_data_unchanged': True,
                           'literal_pool_changes': changed_pools,
                           'changed_switch_target_addends': changed_targets,
                           'switch_target_scope': 'same relocation entries and rewritten function owner; addends retained, source switch arms unchanged; no whole-body equivalence inferred'}
            cells += 1
            bodies += len(grades)
        report[family] = rows
    report['denominator'] = {'compiled_cells': cells, 'shared_body_verdicts': bodies}
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print(cells, 'cells /', bodies, 'body verdicts; metadata/data/relocation audit passes')


if __name__ == '__main__':
    main()
