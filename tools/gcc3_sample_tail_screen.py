#!/usr/bin/env python3
"""Coarse read-only screen for small callers containing wide/narrow operations.

The three opcodes can belong to different values or precede different calls.
Every result needs manual operand/CFG tracing; this is not a dataflow proof,
comparison relaxation, or prediction that a type change will recover a body.
"""
import argparse
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_alignment_stack_screen import inventory

def candidate(row):
    if row['size'] > 300:
        return False
    stream = row['original']
    return any(mn == 'call' and
               {'movzwl', 'neg', 'movswl'} <= {m for m, _ in stream[i+1:]}
               for i, (mn, _) in enumerate(stream))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir', type=Path, default=d.ROOT/'build/production-before')
    parser.add_argument('--output', type=Path, default=d.ROOT/'build/sample-tail-screen.json')
    args = parser.parse_args()
    objects = sorted(args.baseline_dir.glob('*.o'))
    assert len(objects) == 300
    ref = inventory(Path(d.b.BLOB))
    # Real E1u witness and an instruction-list refusal control. These do not
    # compile or mutate reconstruction source.
    key = '_ZN18V92Phase4Modulator11generateE1uEv'
    assert candidate(ref[key])
    control = dict(ref[key], original=[r for r in ref[key]['original'] if r[0] != 'neg'])
    assert not candidate(control)
    rows = []
    common = eligible = exact = 0
    for obj in objects:
        for name, ours in inventory(obj).items():
            if name not in ref:
                continue
            common += 1
            original = ref[name]
            if not candidate(original):
                continue
            eligible += 1
            grade = d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))
            if grade[0] == 'EXACT':
                exact += 1
                continue
            rows.append({'object': obj.name, 'symbol': name, 'grade': grade,
                         'blob_size': original['size'], 'ours_size': ours['size'],
                         'witness': original['original']})
    result = {'objects': len(objects), 'copies': common, 'eligible_copies': eligible,
              'exact_eligible_copies': exact, 'candidates': rows,
              'real_E1u_positive': True, 'missing_NEG_refused': True,
              'limitations': 'Opcodes anywhere after a call; no same-value or path proof.'}
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(common, 'common copies;', eligible, 'eligible;', exact, 'already exact;',
          len(rows), 'nonexact candidates;1 real positive/1 instruction-list refusal')

if __name__ == '__main__':
    main()
