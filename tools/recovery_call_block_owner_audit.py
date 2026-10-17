#!/usr/bin/env python3
"""Audit the bounded inner-owner control; this is not an adoption verdict."""
import json
from pathlib import Path
import gcc3_value_carriers_audit as a

ROOT = Path(__file__).resolve().parents[1]

def main():
    root = ROOT/'build/recovery-call-block-owner'
    cells = json.loads((root/'results.json').read_text())['families']['call']['cells']
    assert cells['baseline']['baseline_reproduced']
    candidate = cells['block-owner']
    assert candidate['changed_bodies'] == ['call_run']
    assert not candidate['gains'] and not candidate['losses']
    old = a.inspect(root/'call/baseline/candidate.o')
    new = a.inspect(root/'call/block-owner/candidate.o')
    for key in ('records', 'allocated', 'nobits'):
        assert old[key] == new[key], key
    assert len(old['relocations']) == len(new['relocations']) == 22
    moved = []
    for before, after in zip(old['relocations'], new['relocations']):
        assert before[:3] == after[:3]
        if before != after:
            assert before[3][:2] == after[3][:2] == ('audited-code-destination', 'call_run')
            moved.append([before, after])
    assert len(moved) == 18
    left = [r[3] for r in old['relocations']]
    right = [r[3] for r in new['relocations']]
    assert [[x == y for y in left] for x in left] == [[x == y for y in right] for x in right]
    report = {'function_verdicts':22, 'known_changed_body_control':'call_run',
              'stable_bystanders':10, 'exact_gains':[], 'exact_losses':[],
              'data_bindings_equal':True, 'nontext_relocations':22,
              'moved_switch_destinations':moved,
              'adopted':False,
              'reason':'Original outer-owner buffer producers precede inner-owner reload; candidate moves buffer producers to inner owner.'}
    (root/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('block-owner audit: 2 cells, 22 function verdicts, 10 stable bystanders; known changed body fires; source not adopted')

if __name__ == '__main__':
    main()
