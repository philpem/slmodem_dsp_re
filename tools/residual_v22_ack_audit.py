#!/usr/bin/env python3
"""Audit eight ACK controls, their RTL lifetime witnesses and full-TU effects."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import chunks, sets, fingerprint
from gcc3_reload_trace import instructions


def named(node, name):
    return isinstance(node, list) and name in node and '[' in node


def main():
    rows = []
    verdicts = 0
    for package in ('residual-v22-ack', 'residual-v22-ack-count'):
        root = d.ROOT / 'build' / package / 'v22prc'
        family = json.loads((root.parent / 'results.json').read_text())['families']['v22prc']
        baseline = root / 'baseline/candidate.o'
        metadata = inspect(baseline)
        assert family['cells']['baseline']['baseline_reproduced']
        for label, cell in family['cells'].items():
            obj = root / label / 'candidate.o'
            actual = inspect(obj)
            assert all(actual[k] == metadata[k] for k in metadata if k != 'text_positions')
            changed = [n for n in cell['functions']
                       if d.b.body(str(obj), n) != d.b.body(str(baseline), n)]
            assert set(changed) <= {'Detect_Rmloop2_ACK', 'Detect_1s'}
            assert not cell.get('gains') and not cell.get('losses')
            stages = {}
            for stage in ('01.rtl', '09.loop', '24.lreg', '25.greg', '27.flow2',
                          '28.peephole2', '30.rnreg', '31.bbro', '33.sched2'):
                selected = chunks(root / label / ('v22prc.c.' + stage), 'Detect_Rmloop2_ACK')
                assert len(selected) == 1
                stages[stage] = instructions(selected[0])
            early = stages['01.rtl']
            definitions = [(uid, assignment) for uid, pattern in early.items()
                           for assignment in sets(pattern)]
            n = [uid for uid, x in definitions if named(x[1], 'n')]
            ms = [uid for uid, x in definitions if named(x[1], 'ms')
                  and x[2][:2] == ['const_int', '0']]
            square = [uid for uid, x in definitions if named(x[1], 'idealEnergy')]
            samples = [uid for uid, x in definitions if x[2][0] == 'mem:HI'
                       and 'sym' in str(x[2])]
            assert len(n) == len(ms) == 1 and len(samples) == 2
            source = (root / label / 'v22prc.c').read_text()
            _, _, fn = d.function(source, 'Detect_Rmloop2_ACK')
            explicit = 'int idealEnergy' in fn
            late = fn.index('unsigned short ms') > fn.index('for (i = 0')
            assert bool(square) == explicit
            if explicit:
                assert len(square) == 1 and square[0] < min(samples)
                after_count = fn.index('unsigned short n') > fn.index('int idealEnergy')
                assert (n[0] > square[0]) == after_count
            assert (ms[0] > max(samples)) == late
            rows.append({'package': package, 'cell': label, 'changed': changed,
                         'bytes': d.b.sizes(str(obj))['Detect_Rmloop2_ACK'],
                         'verdict': cell['verdicts']['Detect_Rmloop2_ACK'],
                         'stage_instruction_counts': {s: len(v) for s, v in stages.items()},
                         'early_n_uid': n[0], 'early_zero_ms_uid': ms[0],
                         'early_ideal_energy_uids': square, 'early_sample_uids': samples})
            verdicts += len(cell['verdicts'])
    first = d.ROOT / 'build/residual-v22-ack/v22prc'
    second = d.ROOT / 'build/residual-v22-ack-count/v22prc'
    for old, new in (('baseline', 'baseline'), ('square-1-late-1', 'combined-repeat')):
        assert (first / old / 'candidate.o').read_bytes() == (second / new / 'candidate.o').read_bytes()
    bystander = {}
    for stage in ('01.rtl', '24.lreg', '25.greg', '27.flow2', '28.peephole2', '30.rnreg'):
        left = instructions(chunks(first / 'baseline' / ('v22prc.c.' + stage), 'Detect_1s')[0])
        right = instructions(chunks(first / 'square-1-late-1' / ('v22prc.c.' + stage), 'Detect_1s')[0])
        assert left.keys() == right.keys()
        bystander[stage] = [uid for uid in left if fingerprint(left[uid]) != fingerprint(right[uid])]
    assert not bystander['01.rtl'] and not bystander['24.lreg']
    assert len(bystander['25.greg']) == 12
    assert len(rows) == 8 and verdicts == 80
    report = {'cells': rows, 'common_body_verdicts': verdicts, 'strict_gains': 0,
              'exact_losses': 0, 'metadata_nontext_equal': True, 'source_adopted': False,
              'following_detector_stage_differences': bystander}
    (d.ROOT / 'build/residual-v22-ack-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    for row in rows:
        print(row['package'], row['cell'], row['bytes'], row['verdict'], row['changed'])
    print('8 full-TU cells / 80 verdicts; lifetime witnesses fire; no adoption')


if __name__ == '__main__':
    main()
