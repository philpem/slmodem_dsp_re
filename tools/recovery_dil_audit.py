#!/usr/bin/env python3
"""Audit complete DIL experiment objects and independent topology witnesses."""
import argparse
import hashlib
import json
import re
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection


def fixed_fields(instructions):
    return sorted({int(match[1], 16)
                   for mnemonic, operand in instructions
                   if mnemonic == 'movzbl'
                   for match in [re.match(r'0x(10[3-9a-f]|11[0-2])\(%(?!esp)[a-z]+\),', operand)]
                   if match})


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run', default='recovery-dil-topology')
    args = ap.parse_args()
    root = d.ROOT / 'build' / args.run
    ledger = json.loads((root / 'results.json').read_text())
    report, cells, bodies = {}, 0, 0
    for family, record in ledger['families'].items():
        baseline = root / family / 'baseline/candidate.o'
        metadata, data = inspect(baseline), allocated(baseline)
        rows = {}
        for label, cell in record['cells'].items():
            path = root / family / label / 'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            current = inspect(path)
            with path.open('rb') as stream:
                elf = ELFFile(stream)
                nontext_relocations = [section.name for section in elf.iter_sections()
                                       if isinstance(section, RelocationSection)
                                       and section.num_relocations()
                                       and elf.get_section(section['sh_info'])['sh_flags'] & 2
                                       and elf.get_section(section['sh_info']).name != '.text']
                assert not nontext_relocations, (family, label, nontext_relocations)
            assert current['records'] == metadata['records'], (family, label, 'symbol metadata')
            assert current['objects'] == metadata['objects'], (family, label, 'named data')
            candidate_data = allocated(path)
            assert set(candidate_data) == set(data)
            changed_pools = {}
            for section, contents in candidate_data.items():
                if contents == data[section]:
                    continue
                assert section.startswith('.rodata.str'), (family, label, section)
                literals = lambda payload: sorted(x for x in bytes.fromhex(payload).split(b'\0') if x)
                assert literals(contents) == literals(data[section]), (family, label, section, 'literal contents')
                changed_pools[section] = {'baseline': data[section], 'candidate': contents}
            assert cell['globals'] == record['cells']['baseline']['globals']
            target = next(iter(cell['verdicts']))
            assert len(cell['verdicts']) == len(cell['functions']) == 1
            original = d.b.insns(d.b.BLOB, target)
            retained = d.b.insns(str(path), target)
            fields = fixed_fields(retained)
            expected = list(range(0x103, 0x113))
            original_fields = fixed_fields(original)
            assert original_fields == expected
            altered = [(m, o.replace('0x103(', '0x203(')) for m, o in original]
            assert fixed_fields(altered) != expected
            if args.run == 'recovery-dil-topology':
                if 'expand-1-' in label:
                    assert fields == expected, (family, label, fields)
                else:
                    assert fields == [], (family, label, fields)
            grade = d.b.verdict(*d.b.body(d.b.BLOB, target), *d.b.body(str(path), target))
            assert list(grade) == cell['verdicts'][target]
            rows[label] = {'grade': grade, 'fixed_descriptor_offsets': fields,
                           'object_sha256': cell['object_hash'],
                           'exact_gains': cell.get('gains', []), 'exact_losses': cell.get('losses', []),
                           'symbol_metadata_named_data_unchanged': True,
                           'literal_pool_order_changes': changed_pools,
                           'other_allocated_nontext_unchanged': True,
                           'nontext_relocations': 0,
                           'original_field_positive_changed_offset_negative': True}
            cells += 1
            bodies += len(cell['verdicts'])
        report[family] = rows
    report['denominator'] = {'cells': cells, 'shared_body_verdicts': bodies}
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print(cells, 'cells /', bodies, 'shared bodies; complete metadata/data audit passes')


if __name__ == '__main__':
    main()
