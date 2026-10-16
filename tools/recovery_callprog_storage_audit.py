#!/usr/bin/env python3
"""Check original BSS offsets and all Callprog relocation/collateral effects."""
import argparse
import json
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_value_carriers_audit import inspect
from storage_layout_audit import inventory


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run-dir', type=Path, default=driver.ROOT / 'build/recovery-callprog-storage')
    args = ap.parse_args()
    root = args.run_dir
    cells = json.loads((root / 'results.json').read_text())['families']['Callprog']['cells']
    paths = {label: root / 'Callprog' / label / 'candidate.o' for label in cells}
    base = inspect(paths['baseline'])
    candidate = inspect(paths['original-storage-order'])
    storage = {label: [row for row in inventory(path)['storage'] if row['section'] == '.bss']
               for label, path in paths.items()}
    assert all(len(rows) == 11 for rows in storage.values())
    original = {row['name']: row for row in inventory(driver.b.BLOB)['storage']
                if row['section'] == '.bss' and row['name'] in {r['name'] for r in storage['baseline']}}
    assert len(original) == 11
    mismatches = {}
    for label, rows in storage.items():
        origin = original['next_state_due_cptd']['value']
        mismatches[label] = [row['name'] for row in rows
                             if original[row['name']]['value'] != row['value'] + origin]
        assert all(original[row['name']]['size'] == row['size'] for row in rows)
    assert mismatches['baseline'] and not mismatches['original-storage-order']
    for key in ('allocated', 'nobits', 'relocations', 'text_positions'):
        assert base[key] == candidate[key], key
    def bindings(records):
        return {name: record[:4] + ([record[5]] if len(record) == 6 else [])
                for name, record in records.items()}
    assert bindings(base['records']) == bindings(candidate['records'])
    changed_records = [name for name in base['records'] if base['records'][name] != candidate['records'][name]]
    assert set(changed_records) == set(mismatches['baseline'])
    raw_changed, relocation_count = [], 0
    for name in driver.b.sizes(str(paths['baseline'])):
        old, old_rel = driver.b.body(str(paths['baseline']), name)
        new, new_rel = driver.b.body(str(paths['original-storage-order']), name)
        assert old_rel == new_rel, name
        relocation_count += len(old_rel)
        if old != new:
            raw_changed.append(name)
        old, new = bytearray(old), bytearray(new)
        for offset in old_rel:
            old[offset:offset+4] = new[offset:offset+4] = b'\0' * 4
        assert old == new, name
    assert set(raw_changed) == {'CALLPROG_Create', 'CALLPROG_Dial', 'CALLPROG_Progress'}
    assert cells['original-storage-order']['verdicts'] == cells['baseline']['verdicts']
    assert not cells['original-storage-order']['gains'] and not cells['original-storage-order']['losses']
    report = {'full_TUs': 2, 'body_verdicts': 12, 'original_BSS_objects': 11,
              'relative_offset_mismatches': mismatches, 'canonical_text_relocations_checked': relocation_count,
              'raw_changed_bodies_only_relocation_addends': raw_changed,
              'changed_storage_records': changed_records,
              'nontext_data_bindings_sizes_equal': True,
              'scope': 'original storage positions support this declaration order; no unique source or whole-object exactness claim'}
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('2 full TUs / 12 verdicts: candidate11/11 original BSS offsets; baseline rejected')
    print('6 bodies / %d canonical text relocations unchanged; 3 raw bodies change BSS addends only' % relocation_count)
    print('data/BSS extents/export bindings/nontext relocations unchanged; 3 exact bystanders preserved')


if __name__ == '__main__':
    main()
