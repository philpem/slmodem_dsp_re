#!/usr/bin/env python3
"""Nominate same-graph stack displacement bijections; never relax byte grading."""
import hashlib
import argparse
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_adid_phase_grouping_audit import TARGET

MEM = re.compile(r'(?<![\w@])(?:-?0x[0-9a-f]+)?\(%esp\)')


def witness(left, right, strict=True):
    left = [(m, o) for m, o in left]
    right = [(m, o) for m, o in right]
    normalize = lambda rows: [(m, MEM.sub('STACK(%esp)', o)) for m, o in rows]
    if d.b.alpha_why(normalize(left), normalize(right)) is not None:
        return None
    forward, reverse, changes = {}, {}, []
    bijective = True
    for index, ((m, a), (_, z)) in enumerate(zip(left, right)):
        first, second = MEM.findall(a), MEM.findall(z)
        if len(first) != len(second):
            return None
        for x, y in zip(first, second):
            if forward.get(x, y) != y or reverse.get(y, x) != x:
                bijective = False
                if strict:
                    return None
            forward[x], reverse[y] = y, x
        if first != second:
            changes.append([index, m, a, z])
    if not changes:
        return None
    save_only = all(m == 'mov' and
                    re.fullmatch(r'(?:%e(?:bx|si|di|bp),.*\(%esp\)|.*\(%esp\),%e(?:bx|si|di|bp))', a)
                    for _, m, a, _ in changes)
    return {'mapping': forward, 'changed_stack_rows': changes,
            'save_area_only': save_only, 'bijection_consistent': bijective}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adid-baseline', type=Path,
                        help='explicit pinned ADID object overlay when production has advanced')
    args = parser.parse_args()
    assert not d.b._staleness()[0]
    ledger = json.loads((d.ROOT / 'build/residual-spill-inventory.json').read_text())
    assert ledger['revision'].startswith('9aa118e1')
    candidates = []
    for row in ledger['candidates']:
        path = d.ROOT / 'build/tc_out' / row['worst_object']
        if args.adid_baseline and row['worst_object'] == 'src_pump_v90_V90AutoDigitalImpDetector.cpp.o':
            path = args.adid_baseline
        assert hashlib.sha256(path.read_bytes()).hexdigest() == ledger['object_sha256'][row['worst_object']]
        result = witness(row['original_instructions'], d.b.insns(str(path), row['symbol']), strict=False)
        if result:
            candidates.append({'symbol': row['symbol'], 'source': row['source'],
                               'grade': row['grade'], **result})
    original = d.b.insns(d.b.BLOB, TARGET)
    positive = d.b.insns(str(d.ROOT / 'build/gcc3-adid-phase-definition-age/V90AutoDigitalImpDetector/best-first-0-float-first-1/candidate.o'), TARGET)
    proof = witness(original, positive)
    assert proof and len(proof['changed_stack_rows']) == 8 and not proof['save_area_only']
    assert witness(original, original) is None
    bad = list(positive)
    bad[0] = ('unexpected', bad[0][1])
    assert witness(original, bad) is None
    bad = list(positive)
    index, mnemonic, _, operand = proof['changed_stack_rows'][0]
    bad[index] = (mnemonic, operand.replace('0x20(%esp)', '0x24(%esp)'))
    assert witness(original, bad) is None
    assert {r['symbol'] for r in candidates} == {'SDM_init', 'FPM_SDM_init'}
    assert all(r['save_area_only'] for r in candidates)
    assert not any(r['bijection_consistent'] for r in candidates)
    result = {'revision': ledger['revision'], 'screened_copies': len(ledger['candidates']),
              'explicit_adid_baseline': str(args.adid_baseline) if args.adid_baseline else None,
              'unique_residual_scope': ledger['unique_census'], 'nominations': candidates,
              'controls': 4, 'reload_spill_candidates': 0,
              'limits': 'Same-graph bijections in REGALLOC/BYTES worst copies only. SIZE/other graphs excluded; no original declaration proof or source adoption.'}
    (d.ROOT / 'build/residual-spill-screen.json').write_text(json.dumps(result, indent=2) + '\n')
    print(len(ledger['candidates']), 'worst copies; 2 save-area-only nominations; 0 reload-slot leads; 4 controls fire')


if __name__ == '__main__':
    main()
