#!/usr/bin/env python3
"""Audit the PSD switch control, ordered dispatch and all shared TU bodies."""
import argparse
import hashlib
import json
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked

TARGET = '_ZN3Psd7processEPfjS0_NS_12OutputOptionE'


def dispatch(path):
    ins = d.b.insns(str(path), TARGET)
    indexes = [i for i, row in enumerate(ins) if row == ('cmpl', '$0x1,0x50(%esp)')]
    assert len(indexes) == 1
    i = indexes[0]
    return ins[i:i+10]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--integrated', action='store_true')
    args = ap.parse_args()
    root = d.ROOT / 'build/gcc3-psd-dispatch'
    cells = json.loads((root / 'results.json').read_text())['families']['psd']['cells']
    base = root / 'psd/baseline/candidate.o'
    assert base.read_bytes() == (d.ROOT / 'build/gcc3-size-spill-lifetime/psd/state-before-division/candidate.o').read_bytes()
    report = {}
    original = dispatch(d.b.BLOB)
    for label, cell in cells.items():
        path = root / 'psd' / label / 'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        assert inspect(path) == inspect(base) and allocated(path) == allocated(base)
        changed = [n for n in cell['functions'] if masked(path, n) != masked(base, n)]
        assert changed == ([TARGET] if label == 'switch' else [])
        grades = {n: d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(path), n))
                  for n in cell['verdicts']}
        assert {n: list(v) for n, v in grades.items()} == cell['verdicts']
        assert grades[TARGET] == ('SIZE', 17 if label == 'switch' else 9)
        assert not cell.get('gains') and not cell.get('losses')
        graph = dispatch(path)
        if label == 'switch':
            assert [m for m, o in graph] == [m for m, o in original]
            assert [o for m, o in graph if not m.startswith('j')] == [o for m, o in original if not m.startswith('j')]
            assert graph[1] == original[1] == ('je', '.+385')
        report[label] = {'verdicts': grades, 'changed_bodies': changed, 'dispatch': graph,
                         'metadata_data_unchanged': True}
    if args.integrated:
        winner = root / 'psd/switch'
        assert (d.ROOT / 'src/dsp/psd.cpp').read_bytes() == (winner / 'psd.cpp').read_bytes()
        assert (d.ROOT / 'build/tc_out/src_dsp_psd.cpp.o').read_bytes() == (winner / 'candidate.o').read_bytes()
    report['original_dispatch'] = original
    report['denominator'] = {'source_cells': 2, 'common_body_verdicts': 26}
    report['integrated'] = args.integrated
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('2 cells /26 body verdicts; ordered original dispatch recovered; process SIZE17 remains; 12 bystanders/data unchanged')


if __name__ == '__main__':
    main()
