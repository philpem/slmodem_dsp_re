#!/usr/bin/env python3
"""Rescore callback/value-age controls and audit whole translation units."""
import hashlib
import json
from collections import Counter
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

PACKAGES = {
    'prefilter-reload-dispatch': {'_ZN12V90PreFilter12selectFilterEv'},
    'v17-control-value-graph': {'V17TX_control'},
    'v17-control-scale-publication': {'V17TX_control'},
    'fax-control-age-transfer': {'V27TX_control', 'V29TX_control'},
    'v29-control-flag-verdict': {'V29TX_control'},
    'fax-status-flag-age': {'V17TX_status', 'V29TX_status'},
    'fax-status-common-result': {'fax_class1_status'},
    'fax-status-dispatch': {'fax_class1_status'},
}
EXPECTED = {('v17-control-scale-publication', 'V17t_stc', 'capture-original-store'): ['V17TX_control'],
            ('fax-control-age-transfer', 'V27t_stc', 'scale1-flags1'): ['V27TX_control'],
            ('fax-status-dispatch', 'class1', 'switch-shared'): ['fax_class1_status']}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    report = {'cells': [], 'body_grades': 0, 'raw_baselines': 0, 'gains': [], 'losses': []}
    for package, allowed in PACKAGES.items():
        root = d.ROOT/'build'/package
        ledger = json.loads((root/'results.json').read_text())
        assert ledger['revision'] == '9025b8d8'
        for family, data in ledger['families'].items():
            base = root/family/'baseline/candidate.o'
            retained = d.ROOT/'build/reload-baseline'/(data['source_path'].replace('/', '_')+'.o')
            assert base.read_bytes() == retained.read_bytes()
            report['raw_baselines'] += 1
            metadata = inspect(base)
            names = sorted(d.b.sizes(str(base)))
            for label, cell in data['cells'].items():
                folder = root/family/label
                obj = folder/'candidate.o'
                assert cell['compile_exit'] == 0 and digest(obj) == cell['object_hash']
                assert digest(folder/Path(data['source_path']).name) == cell['source_hash']
                assert names == cell['functions'] == sorted(d.b.sizes(str(obj)))
                grades = {n: list(d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(obj), n))) for n in names}
                assert grades == cell['verdicts']
                report['body_grades'] += len(names)
                changed = sorted(n for n in names if d.b.body(str(base), n) != d.b.body(str(obj), n))
                assert set(changed) <= allowed, (package, label, changed)
                assert not cell.get('losses')
                expected = EXPECTED.get((package, family, label), [])
                assert cell.get('gains', []) == expected
                report['gains'].extend(expected)
                current = inspect(obj)
                for key in ('records', 'nobits', 'relocations'):
                    assert current[key] == metadata[key], (package, family, label, key)
                assert set(current['allocated']) == set(metadata['allocated'])
                changed_data = [n for n in current['allocated'] if current['allocated'][n] != metadata['allocated'][n]]
                if changed_data:
                    assert package == 'prefilter-reload-dispatch' and changed_data == ['.rodata.str1.4']
                    before = bytes.fromhex(metadata['allocated'][changed_data[0]])
                    after = bytes.fromhex(current['allocated'][changed_data[0]])
                    assert len(before) == len(after)
                    assert Counter(x for x in before.split(b'\0') if x) == Counter(x for x in after.split(b'\0') if x)
                report['cells'].append({'package': package, 'family': family, 'label': label,
                                        'changed_bodies': changed, 'gains': expected,
                                        'reviewed_merge_string_reorder': changed_data})
    assert report['gains'] == ['V17TX_control', 'V27TX_control', 'fax_class1_status']
    (d.ROOT/'build/callback-value-age-batch-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f"Callback age audit: {len(report['cells'])} full-TU cells / {report['body_grades']} live grades / "
          f"{report['raw_baselines']} raw baselines; 3 strict gains / 0 losses")
    print('Known detector: V17 delayed scale publication, V27 fresh flag age, grouped status switch each closes a complete body')
    print('Bindings/BSS/nontext fixed; only reviewed PreFilter negative-control string-pool reorder')


if __name__ == '__main__':
    main()
