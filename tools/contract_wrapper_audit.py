#!/usr/bin/env python3
"""Recheck live full-TU objects from bounded wrapper unsigned controls."""
import hashlib
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect


def main():
    rows = []
    for package in ('contract-wrapper-unsigned', 'contract-wrapper-fragment', 'fse-config-copy', 'tone-reversal-energy'):
        root = d.ROOT / 'build' / package
        results = json.loads((root / 'results.json').read_text())
        for family, content in results['families'].items():
            baseline_path = root / family / 'baseline/candidate.o'
            baseline = inspect(baseline_path)
            assert content['cells']['baseline']['baseline_reproduced']
            for label, cell in content['cells'].items():
                path = root / family / label / 'candidate.o'
                assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
                current = inspect(path)
                for key in ('records', 'allocated', 'nobits', 'relocations'):
                    assert current[key] == baseline[key], (package, label, key)
                changed = []
                for name in cell['functions']:
                    actual = d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(path), name))
                    assert list(actual) == cell['verdicts'][name], (package, label, name, actual)
                    if d.b.body(str(path), name) != d.b.body(str(baseline_path), name):
                        changed.append(name)
                assert set(changed) <= {'dp_wrapper_run', 'FPM_FSE_init', 'FPM_TONE_find_rev'}
                assert not cell.get('gains', []) and not cell.get('losses', [])
                rows.append({'package': package, 'label': label,
                             'bodies': len(cell['functions']), 'changed': changed,
                             'verdict': cell['verdicts'].get('dp_wrapper_run', cell['verdicts'].get('FPM_FSE_init', cell['verdicts'].get('FPM_TONE_find_rev')))})
    changed_controls = sum(bool(row['changed']) for row in rows)
    assert changed_controls == 8, changed_controls
    result = {'cells': len(rows), 'body_comparisons': sum(r['bodies'] for r in rows),
              'changed_controls': changed_controls, 'gains': 0, 'losses': 0, 'rows': rows}
    (d.ROOT / 'build/contract-wrapper-audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('wrapper/FSE audit:', result['cells'], 'cells;', result['body_comparisons'],
          'live function comparisons;', changed_controls, 'known changed-body controls; 0 gains/0 losses')


if __name__ == '__main__':
    main()
