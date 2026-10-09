#!/usr/bin/env python3
"""Audit the V32 getter and V22 counter-use complete-TU controls."""
import json
from pathlib import Path
import gcc3_value_carriers_audit as a
import playbook_small_patterns as d

def main():
    rows = []
    for package, family, symbol, count in (
        ('v32-cleaned-path', 'V32', 'V32FP_GetCleanedSamples', 2),
        ('v22-mrf-counter-preload', 'v22_mrf', 'V22_MRF_init', 6),
    ):
        root = d.ROOT/'build'/package
        cells = json.loads((root/'results.json').read_text())['families'][family]['cells']
        assert len(cells) == count
        baseline = root/family/'baseline/candidate.o'
        assert cells['baseline']['baseline_reproduced']
        before = a.inspect(baseline)
        for label, cell in cells.items():
            obj = root/family/label/'candidate.o'
            after = a.inspect(obj)
            for key in ('records', 'allocated', 'nobits', 'relocations'):
                assert before[key] == after[key], (package, label, key)
            assert not cell.get('gains') and not cell.get('losses')
            if label != 'baseline':
                assert cell['changed_bodies'] == [symbol], (package, label)
            rows.append({'package': package, 'cell': label,
                         'target': cell['verdicts'][symbol],
                         'body_grades': len(cell['verdicts']),
                         'data_metadata_relocations_identical': True,
                         'changed_bodies': cell.get('changed_bodies', [])})
    report = {'cells': len(rows), 'body_grades': sum(r['body_grades'] for r in rows),
              'strict_gains': 0, 'strict_losses': 0, 'rows': rows}
    root = d.ROOT/'build/rms-reciprocal-operands'
    cells = json.loads((root/'results.json').read_text())['families']['Beepgen']['cells']
    assert len(cells) == 3 and cells['baseline']['baseline_reproduced']
    baseline = root/'Beepgen/baseline/candidate.o'
    before = a.inspect(baseline)
    for label, cell in cells.items():
        obj = root/'Beepgen'/label/'candidate.o'
        after = a.inspect(obj)
        for key in ('records', 'nobits', 'relocations'):
            assert before[key] == after[key], (label, key)
        if label == 'native-double-literal':
            expected = ['CrossDataLinks', 'FDSP_DP_Run', 'bSearchEnergy', 'fComputeRMSValueFloatBuf']
            assert cell['changed_bodies'] == expected
            assert set(before['allocated']) == set(after['allocated'])
            for section, data in before['allocated'].items():
                if section == '.rodata.cst4':
                    # The only vanished pooled constant is the unused SF one.
                    assert data[32:40] == '0000803f'
                    assert after['allocated'][section] == data[:32]+data[40:]
                else:
                    assert data == after['allocated'][section], section
        else:
            assert obj.read_bytes() == baseline.read_bytes()
        assert not cell.get('gains') and not cell.get('losses')
        report['rows'].append({'package': 'rms-reciprocal-operands', 'cell': label,
                               'target': cell['verdicts']['fComputeRMSValueFloatBuf'],
                               'body_grades': len(cell['verdicts']),
                               'changed_bodies': cell.get('changed_bodies', [])})
    report['cells'] = len(report['rows'])
    report['body_grades'] = sum(r['body_grades'] for r in report['rows'])
    (d.ROOT/'build/small-counter-use-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))

if __name__ == '__main__':
    main()
