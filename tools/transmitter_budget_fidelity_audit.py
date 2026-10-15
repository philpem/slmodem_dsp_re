#!/usr/bin/env python3
"""Audit fourteen budget and partial-boundary controls, including bystanders."""
from collections import Counter
import hashlib
import json
import re
import playbook_small_patterns as d
from batch50_fax_budget import NAMES
from gcc3_value_carriers_audit import inspect
from helper_caller_audit import inspect as named
from gcc3_adid_first_study_audit import masked
from narrow_call_boundary_screen import bodies


def main():
    root = d.ROOT / 'build/transmitter-budget-fidelity'
    ledger = json.loads((root / 'results.json').read_text())
    report = {}
    cells = verdicts = 0
    families = [(root, family, record) for family, record in ledger['families'].items()]
    from transmitter_v27_budget_reproduce import NAMES as v27_names
    NAMES['V27t_prc'] = v27_names
    v27_root = d.ROOT / 'build/transmitter-v27-budget'
    v27 = json.loads((v27_root / 'results.json').read_text())
    families += [(v27_root, family, record) for family, record in v27['families'].items()]
    for root, family, record in families:
        baseline = root / family / 'baseline/candidate.o'
        assert baseline.read_bytes() == (root / family / 'retained.o').read_bytes()
        base = inspect(baseline)
        rows = {}
        for label, entry in record['cells'].items():
            path = root / family / label / 'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == entry['object_hash']
            assert hashlib.sha256((path.parent / (family+'.c')).read_bytes()).hexdigest() == entry['source_hash']
            now = inspect(path)
            for key in ('records', 'allocated', 'nobits', 'relocations'):
                assert now[key] == base[key], (family, label, key)
            assert named(path)['objects'] == named(baseline)['objects']
            changed = [n for n in entry['functions'] if masked(path, n) != masked(baseline, n)]
            assert set(changed) == (set(NAMES[family]) if label != 'baseline' else set()), changed
            assert not entry.get('gains') and not entry.get('losses')
            for name in changed:
                edges = lambda p: Counter(r['target'] for r in bodies(p)[name] if 'target' in r)
                assert edges(path) == edges(baseline), (family, name, 'calls')
            rows[label] = {'changed': changed, 'verdicts': entry['verdicts'],
                           'metadata_data_nontext_calls_bystanders': 'pass'}
            cells += 1
            verdicts += len(entry['verdicts'])
        report[family] = rows
    assert (cells, verdicts) == (6, 62)
    from callback_control_audit import coefficient_age
    assert coefficient_age(d.b.BLOB) == 'before-transfer'
    extra = d.ROOT / 'build/partial-loop-snapshot'
    ledger = json.loads((extra / 'results.json').read_text())
    for family, record in ledger['families'].items():
        baseline = extra / family / 'baseline/candidate.o'
        assert baseline.read_bytes() == (extra / family / 'retained.o').read_bytes()
        base = inspect(baseline)
        target = 'V29RX_control' if family == 'V29r_stc' else 'fskdetect'
        rows = {}
        for label, entry in record['cells'].items():
            path = extra / family / label / 'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == entry['object_hash']
            now = inspect(path)
            for key in ('records', 'allocated', 'nobits', 'relocations'):
                assert now[key] == base[key], (family, label, key)
            assert named(path)['objects'] == named(baseline)['objects']
            changed = [n for n in entry['functions'] if masked(path, n) != masked(baseline, n)]
            assert set(changed) == ({target} if label != 'baseline' else set())
            assert not entry.get('gains') and not entry.get('losses')
            for name in changed:
                edges = lambda p: Counter(r['target'] for r in bodies(p)[name] if 'target' in r)
                assert edges(path) == edges(baseline)
            proof = {}
            if family == 'V29r_stc':
                insns = d.b.insns(str(path), target)
                proof = {offset: sum(bool(re.search(r'0x'+offset+r'\(%', o)) for m,o in insns)
                         for offset in ('c','d')}
                assert proof == ({'c':2,'d':2} if label == 'baseline' else {'c':1,'d':1}), proof
            elif label != 'baseline':
                proof = {'coefficient_age':coefficient_age(path)}
                assert proof['coefficient_age'] == 'before-transfer'
            rows[label] = {'changed':changed,'verdicts':entry['verdicts'],'operand_proof':proof,
                           'metadata_data_nontext_calls_bystanders':'pass'}
            cells += 1
            verdicts += len(entry['verdicts'])
        report[family] = rows
    assert (cells, verdicts) == (10, 70)
    carrier_root = d.ROOT / 'build/transmitter-v27-carrier/V27t_prc'
    carrier = json.loads((carrier_root.parent / 'results.json').read_text())['families']['V27t_prc']
    baseline = carrier_root / 'baseline/candidate.o'
    assert baseline.read_bytes() == (carrier_root / 'retained.o').read_bytes()
    base = inspect(baseline)
    rows = {}
    for label, entry in carrier['cells'].items():
        path = carrier_root / label / 'candidate.o'
        now = inspect(path)
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry['object_hash']
        for key in ('records','allocated','nobits','relocations'):
            assert now[key] == base[key], (label,key)
        assert named(path)['objects'] == named(baseline)['objects']
        changed = [n for n in entry['functions'] if masked(path,n) != masked(baseline,n)]
        assert set(changed) == ({'TxHdxQuietV27'} if label != 'baseline' else set())
        assert not entry.get('gains') and not entry.get('losses')
        for name in changed:
            edges = lambda p: Counter(r['target'] for r in bodies(p)[name] if 'target' in r)
            assert edges(path) == edges(baseline)
        rows[label] = {'changed':changed,'verdicts':entry['verdicts'],
                       'metadata_data_nontext_calls_bystanders':'pass'}
        cells += 1
        verdicts += len(entry['verdicts'])
    report['V27-Quiet-carrier-cross'] = rows
    assert (cells, verdicts) == (14, 110)
    report['denominator'] = {'complete_TU_cells': cells, 'shared_body_verdicts': verdicts}
    (root / 'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print(cells, 'full-TU cells /', verdicts, 'shared body verdicts; sixteen selected bodies change; no exact losses')


if __name__ == '__main__':
    main()
