#!/usr/bin/env python3
"""Audit the complete paired-error domain and its first-RTL/native boundary."""
import hashlib
import json
import re
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks
from narrow_call_boundary_screen import bodies


def native(path):
    rows = bodies(path)['FPM_FSE_receive']
    positions = [i for i, r in enumerate(rows) if r.get('target') == 'FPM_lmsupd']
    assert len(positions) == 2
    between = rows[positions[0]+1:positions[1]]
    return {'between_calls': between, 'arithmetic': [r for r in between
            if r['mnemonic'] in ('imul', 'sar', 'cwtl', 'movswl')]}


def main():
    root = d.ROOT/'build/gcc3-lms-paired-error/fpm_fse'
    ledger = json.loads((root.parent/'results.json').read_text())['families']['fpm_fse']
    baseline = root/'baseline/candidate.o'
    assert baseline.read_bytes() == (root/'retained.o').read_bytes()
    original = native(d.b.BLOB)
    assert original['arithmetic'] == []
    report = {'original': original, 'cells': {}, 'shared_verdicts': 0}
    for label, entry in ledger['cells'].items():
        path = root/label/'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry['object_hash']
        assert inspect(path) == inspect(baseline)
        assert allocated(path) == allocated(baseline)
        changed = [name for name in entry['functions'] if masked(path,name) != masked(baseline,name)]
        assert set(changed) <= {'FPM_FSE_receive'}
        assert not entry.get('gains') and not entry.get('losses')
        before = [r.get('target') for r in bodies(baseline)['FPM_FSE_receive'] if 'target' in r]
        after = [r.get('target') for r in bodies(path)['FPM_FSE_receive'] if 'target' in r]
        assert before == after
        rtl = chunks(root/label/'fpm_fse.c.01.rtl', 'FPM_FSE_receive')[0]
        positions = [m.start() for m in re.finditer(r'\("FPM_lmsupd"\)', rtl)]
        assert len(positions) == 2
        between = rtl[positions[0]:positions[1]]
        counts = {op: between.count('('+op+':SI') for op in ('mult','ashiftrt')}
        expected = {'mult': 1, 'ashiftrt': 1} if label == 'baseline' else {'mult': 0, 'ashiftrt': 0}
        assert counts == expected, (label, counts)
        output = native(path)
        assert bool(output['arithmetic']) == (label == 'baseline')
        report['cells'][label] = {'changed_bodies': changed, 'verdicts': entry['verdicts'],
                                 'initial_RTL_between_calls': counts, 'native': output,
                                 'metadata_named_allocated_data_calls_bystanders': 'pass'}
        report['shared_verdicts'] += len(entry['verdicts'])
    assert (root/'paired-locals/candidate.o').read_bytes() == (root/'paired-existing/candidate.o').read_bytes()
    assert report['shared_verdicts'] == 12
    (d.ROOT/'build/lms-paired-error-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('3 full-TU cells / 12 shared verdicts; raw baseline, data, calls, bystanders pass')
    print('initial RTL and native inter-call arithmetic: baseline present, both paired controls absent')


if __name__ == '__main__':
    main()
