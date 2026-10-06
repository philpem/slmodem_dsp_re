#!/usr/bin/env python3
"""Recheck bounded FSE diagnostic-result domains and their repeated controls."""
import hashlib
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect


def main():
    packages = ('fse-getdiag-result', 'fse-getdiag-selection', 'fse-getdiag-cap-exit')
    rows = []
    for package in packages:
        root = d.ROOT/'build'/package/'fpm_fse'
        ledger = json.loads((root.parent/'results.json').read_text())
        baseline_path = root/'baseline/candidate.o'
        assert baseline_path.read_bytes() == (root/'retained.o').read_bytes()
        baseline = inspect(baseline_path)
        for label, cell in ledger['families']['fpm_fse']['cells'].items():
            obj = root/label/'candidate.o'
            assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
            assert hashlib.sha256((obj.parent/'fpm_fse.c').read_bytes()).hexdigest() == cell['source_hash']
            current = inspect(obj)
            for key in ('records', 'allocated', 'nobits', 'relocations'):
                assert current[key] == baseline[key], (package, label, key)
            changed = []
            for name in cell['functions']:
                actual = d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))
                assert list(actual) == cell['verdicts'][name]
                if d.b.body(str(obj), name) != d.b.body(str(baseline_path), name):
                    changed.append(name)
            assert set(changed) <= {'FSE_getdiag'}
            assert not cell.get('gains') and not cell.get('losses')
            rows.append({'package': package, 'label': label, 'bodies': len(cell['functions']),
                         'changed': changed, 'verdict': cell['verdicts']['FSE_getdiag']})
    first = d.ROOT/'build/fse-getdiag-result/fpm_fse/entry-result-guard/candidate.o'
    repeat = d.ROOT/'build/fse-getdiag-selection/fpm_fse/result-guard-control/candidate.o'
    assert first.read_bytes() == repeat.read_bytes()
    first = d.ROOT/'build/fse-getdiag-selection/fpm_fse/positive-overflow-arm/candidate.o'
    repeat = d.ROOT/'build/fse-getdiag-cap-exit/fpm_fse/conditional-0-literal-exit-0/candidate.o'
    assert first.read_bytes() == repeat.read_bytes()
    assert len(rows) == 14 and sum(r['bodies'] for r in rows) == 56
    assert sum(bool(r['changed']) for r in rows) == 11
    result = {'cells': 14, 'body_comparisons': 56, 'raw_baselines': 3,
              'cross_domain_raw_repeats': 2, 'known_changed_controls': 11,
              'gains': 0, 'losses': 0, 'rows': rows}
    (d.ROOT/'build/fse-getdiag-domains-audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('FSE diagnostic domains:14valid cells/56live body grades,3raw baselines,2cross-domain raw repeats,11known changed controls; no gains/losses or metadata/data drift')


if __name__ == '__main__':
    main()
