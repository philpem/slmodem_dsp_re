#!/usr/bin/env python3
"""Audit all five call carrier/queue cells, including switch destinations."""
import json
from pathlib import Path
import gcc3_value_carriers_audit as a

ROOT = Path(__file__).resolve().parents[1]

def audit(root):
    cells = json.loads((root/'results.json').read_text())['families']['call']['cells']
    assert cells['baseline']['baseline_reproduced']
    base = a.inspect(root/'call/baseline/candidate.o')
    reports = []
    expected = {'baseline': [], 'unsigned-sregs': ['call_create'],
                'unsigned-queues': ['call_run'],
                'unsigned-both': ['call_create', 'call_run'],
                'unsigned-owner': ['call_create', 'call_run']}
    for label, cell in cells.items():
        obj = a.inspect(root/'call'/label/'candidate.o')
        for key in ('records', 'allocated', 'nobits'):
            assert obj[key] == base[key], (label, key)
        assert sorted(cell.get('changed_bodies', [])) == expected[label]
        assert not cell.get('gains') and not cell.get('losses')
        assert len(cell['verdicts']) == 11
        assert sum(v[0] == 'EXACT' for v in cell['verdicts'].values()) == 7
        assert len(obj['relocations']) == len(base['relocations'])
        changed = []
        for old, new in zip(base['relocations'], obj['relocations']):
            assert old[:3] == new[:3]
            if old != new:
                assert old[3][:2] == new[3][:2] == ('audited-code-destination', 'call_run')
                changed.append([old, new])
        # Offset movement is permitted only for audited instruction-boundary
        # destinations of the unchanged switch partition, never data payload.
        assert len(changed) == (18 if 'queues' in label or label in ('unsigned-both','unsigned-owner') else 0)
        old_targets = [r[3] for r in base['relocations']]
        new_targets = [r[3] for r in obj['relocations']]
        assert [[x == y for y in old_targets] for x in old_targets] == [[x == y for y in new_targets] for x in new_targets]
        reports.append({'cell':label, 'function_verdicts':11, 'exact':7,
                        'changed':expected[label], 'stable_bodies':11-len(expected[label]),
                        'nontext_relocations':len(old_targets), 'moved_switch_destinations':changed})
    # Detector controls: known source changes must actually fire, while the
    # independent s-register and queue axes leave the other body untouched.
    assert cells['unsigned-sregs']['changed_bodies'] == ['call_create']
    assert cells['unsigned-queues']['changed_bodies'] == ['call_run']
    return {'cells':reports, 'denominator_function_verdicts':55,
            'known_positive_axes':2, 'exact_gains':[], 'exact_losses':[]}

if __name__ == '__main__':
    root = ROOT/'build/recovery-call-unsigned'
    result = audit(root)
    (root/'audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('call unsigned audit: 5 cells, 55 function verdicts, 2 positive axis controls; data/bindings/switch partition preserved')
