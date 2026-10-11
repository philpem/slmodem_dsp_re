#!/usr/bin/env python3
"""Audit nine tone controls and positively detect the final sample reload."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import chunks, sets
from gcc3_reload_trace import instructions


def main():
    rows = []
    verdicts = 0
    for package in ('residual-tone-reversal', 'residual-tone-arithmetic'):
        root = d.ROOT / 'build' / package / 'fpm_tone'
        family = json.loads((root.parent / 'results.json').read_text())['families']['fpm_tone']
        baseline = root / 'baseline/candidate.o'
        metadata = inspect(baseline)
        assert family['cells']['baseline']['baseline_reproduced']
        for label, cell in family['cells'].items():
            obj = root / label / 'candidate.o'
            actual = inspect(obj)
            assert all(actual[k] == metadata[k] for k in metadata if k != 'text_positions')
            changed = [n for n in cell['functions'] if d.b.body(str(obj), n) != d.b.body(str(baseline), n)]
            assert set(changed) <= {'FPM_TONE_find_rev'}
            assert not cell.get('gains') and not cell.get('losses')
            stages = {}
            for stage in ('01.rtl', '09.loop', '24.lreg', '25.greg', '27.flow2', '28.peephole2', '30.rnreg', '33.sched2'):
                selected = chunks(root / label / ('fpm_tone.c.' + stage), 'FPM_TONE_find_rev')
                assert len(selected) == 1
                stages[stage] = instructions(selected[0])
            definitions = [(uid, x) for uid, p in stages['01.rtl'].items() for x in sets(p)]
            energy = [(uid, x[2][0]) for uid, x in definitions
                      if isinstance(x[1], list) and 'energy' in x[1]]
            reads = [uid for uid, x in definitions if x[2][0] == 'mem:HI' and 'samples' in str(x[2])]
            age = [uid for uid, x in definitions if x[1][0] == 'mem/s:HI' and '<variable>.rev_age+0' in x[1]]
            source = (root / label / 'fpm_tone.c').read_text()
            _, _, fn = d.function(source, 'FPM_TONE_find_rev')
            sequential = 'energy -= ' in fn
            captured = 'short sample =' in fn
            final_reload = 'hist[idx] = samples[n];' in fn
            assert [op for _, op in energy] == (['sign_extend:SI', 'plus:SI', 'minus:SI'] if sequential
                                               else ['sign_extend:SI', 'plus:SI'])
            assert len(reads) == (3 if not captured else 2 if final_reload else 1)
            assert bool(reads[-1] > max(age)) == final_reload
            rows.append({'package': package, 'cell': label,
                         'bytes': d.b.sizes(str(obj))['FPM_TONE_find_rev'],
                         'verdict': cell['verdicts']['FPM_TONE_find_rev'],
                         'energy_definitions': energy, 'sample_read_uids': reads,
                         'age_write_uids': age, 'final_reload_after_age': final_reload,
                         'stage_instruction_counts': {s: len(v) for s, v in stages.items()},
                         'unchanged_bystanders': len(cell['functions']) - 1})
            verdicts += len(cell['verdicts'])
    first = d.ROOT / 'build/residual-tone-reversal/fpm_tone'
    second = d.ROOT / 'build/residual-tone-arithmetic/fpm_tone'
    for a, b in (('baseline', 'baseline'), ('sequential-1-sample-0', 'sequential-repeat')):
        assert (first / a / 'candidate.o').read_bytes() == (second / b / 'candidate.o').read_bytes()
    assert len(rows) == 9 and verdicts == 99
    assert sum(not row['final_reload_after_age'] for row in rows) == 2
    report = {'cells': rows, 'common_body_verdicts': verdicts, 'strict_gains': 0,
              'source_adopted': False, 'unsupported_capture_controls_detected': 2}
    (d.ROOT / 'build/residual-tone-reversal-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    for row in rows:
        print(row['cell'], row['bytes'], row['verdict'], 'final reload', row['final_reload_after_age'])
    print('9 full-TU cells / 99 verdicts; 10 bystanders preserved; two lost-reload controls detected; no adoption')


if __name__ == '__main__':
    main()
