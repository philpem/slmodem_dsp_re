#!/usr/bin/env python3
"""Audit the complete constructor and P4 sample-width domains, including data.

The P4 switch table is allowed to move only to decoded instruction boundaries
inside the unchanged owning method. All destinations and deltas are retained
in the ledger, never erased for a strict function verdict.
"""
import json
import re
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

def audit(package, family):
    folder = d.ROOT/'build'/package
    cells = json.loads((folder/'results.json').read_text())['families'][family]['cells']
    basepath = folder/family/'baseline/candidate.o'
    base = inspect(basepath)
    names = d.b.sizes(str(basepath))
    reports = []
    assert cells['baseline']['baseline_reproduced']
    for label, cell in cells.items():
        path = folder/family/label/'candidate.o'
        got = inspect(path)
        for key in ['records', 'allocated', 'nobits']:
            assert got[key] == base[key], (package, label, key)
        assert set(d.b.sizes(str(path))) == set(names)
        changed = [n for n in names if d.b.body(str(path), n) != d.b.body(str(basepath), n)]
        assert sorted(changed) == sorted(cell.get('changed_bodies', []))
        assert not cell.get('losses', [])
        relocations = []
        assert len(got['relocations']) == len(base['relocations'])
        for before, after in zip(base['relocations'], got['relocations']):
            assert before[:3] == after[:3]
            if before != after:
                assert package == 'gcc3-p4-sample'
                assert before[3][:2] == after[3][:2] == (
                    'audited-code-destination', '_ZN18V92Phase4Modulator14generateSymbolEv')
                relocations.append({'before': before, 'after': after,
                                    'delta': after[3][2]-before[3][2]})
        record = {'label': label, 'common_body_verdicts': len(cell['verdicts']),
                  'all_emitted_functions': len(names), 'changed_bodies': changed,
                  'gains': cell.get('gains', []), 'losses': cell.get('losses', []),
                  'unchanged_bystanders': len(names)-len(changed),
                  'metadata_allocated_data_BSS_unchanged': True,
                  'decoded_table_destination_changes': relocations}
        if package == 'gcc3-iir-constructor':
            expected = {'_ZN10GenericIIRIfdEC1EjjPdS1_j', '_ZN10GenericIIRIfdEC2EjjPdS1_j'}
            assert set(changed) == (set() if label == 'baseline' else expected)
            assert cell.get('gains', []) == (['_ZN10GenericIIRIfdEC1EjjPdS1_j']
                                          if label == 'count-length-owned' else [])
            text = (folder/family/label/'FloatIIR.cpp.24.lreg').read_text()
            chunks = [c for c in text.split(';; Function ')
                      if c.startswith('GenericIIR<Sample, Coeff>::GenericIIR(')]
            assert len(chunks) == 2 and chunks[0] == chunks[1]
            assert '[ blockSize ]' in chunks[0]
            allocation = int(re.search(r';; Register 63 in (\d+)\.', chunks[0])[1])
            span = int(re.search(r'Register 63 used 3 times across (\d+) insns', chunks[0])[1])
            assert (allocation, span) == ((2, 12) if label == 'count-length-owned' else (4, 16))
            record.update(blockSize_pseudo=63, lreg_hard_register=allocation, lreg_span_insns=span)
        else:
            p = '_ZN18V92Phase4Modulator'
            expected = set()
            if label.startswith('cpt-1'): expected.add(p+'11generateCPtEv')
            if label.endswith('e1u-1'): expected.add(p+'11generateE1uEv')
            if expected: expected.add(p+'14generateSymbolEv')
            assert set(changed) == expected
            assert cell.get('gains', []) == ([p+'11generateE1uEv'] if label.endswith('e1u-1') else [])
            record['target_verdicts'] = {n: cell['verdicts'][n] for n in (
                p+'11generateCPtEv', p+'11generateE1uEv', p+'14generateSymbolEv')}
        reports.append(record)
    result = {'cells': len(reports), 'common_verdicts': sum(r['common_body_verdicts'] for r in reports),
              'all_emitted_body_comparisons': sum(r['all_emitted_functions'] for r in reports),
              'reports': reports}
    (folder/'complete-tu-audit.json').write_text(json.dumps(result, indent=2)+'\n')
    return result

def audit_demod():
    folder = d.ROOT/'build/gcc3-demod-member'
    family = 'V90Phase3Demodulator'
    cells = json.loads((folder/'results.json').read_text())['families'][family]['cells']
    basepath = folder/family/'baseline/candidate.o'
    base = inspect(basepath)
    target = '_ZN20V90Phase3Demodulator13twoLevelDemodEfRi'
    reports = []
    assert len(cells) == 2 and cells['baseline']['baseline_reproduced']
    for label, cell in cells.items():
        path = folder/family/label/'candidate.o'
        got = inspect(path)
        for key in ['records', 'allocated', 'nobits', 'relocations']:
            assert got[key] == base[key], (label, key)
        names = d.b.sizes(str(basepath))
        assert len(names) == len(cell['verdicts']) == 26
        changed = [n for n in names if d.b.body(str(path), n) != d.b.body(str(basepath), n)]
        assert changed == ([] if label == 'baseline' else [target])
        assert not cell.get('gains', []) and not cell.get('losses', [])
        reports.append({'label': label, 'common_body_verdicts': len(names),
                        'all_emitted_functions': len(names), 'changed': changed,
                        'target_verdict': cell['verdicts'][target],
                        'metadata_data_relocations_unchanged': True})
    result = {'cells': 2, 'common_verdicts': 52, 'all_emitted_body_comparisons': 52,
              'reports': reports, 'adopted': False}
    (folder/'complete-tu-audit.json').write_text(json.dumps(result, indent=2)+'\n')
    return result

if __name__ == '__main__':
    reports = [audit('gcc3-iir-constructor', 'FloatIIR'),
               audit('gcc3-p4-sample', 'V92Phase4Modulator'), audit_demod()]
    print(sum(r['cells'] for r in reports), 'complete TU audits;',
          sum(r['common_verdicts'] for r in reports), 'strict common verdicts;',
          sum(r['all_emitted_body_comparisons'] for r in reports), 'emitted body comparisons')
