#!/usr/bin/env python3
"""Verify the bounded narrowing screen's controls and reviewed negatives.

The audit checks local decoded value paths, never global input reachability.
No source compilation or reconstruction change is performed by these tools.
"""
import argparse
import json
from pathlib import Path
import recovery_float_decision_screen as screen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--screen', type=Path, required=True)
    parser.add_argument('--objects', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--period-dir', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    saved = json.loads(args.screen.read_text())
    assert screen.sha(args.reference) == saved['reference_sha256']
    assert all(screen.sha(args.objects/name) == digest for name, digest in saved['objects'].items())
    assert (args.objects/'.build-config').read_text() == saved['config']
    controls = saved['controls']
    assembled = {k: v for k, v in controls.items() if k not in ('tone', 'original_tone')}
    assert len(assembled) == 19
    assert sum(assembled.values()) == 6
    original_tone = controls['original_tone']['decisions']
    original_pattern = [d['narrowed'] for d in original_tone]
    assert original_pattern == [True, False, True]
    for label in ('baseline', 'explicit-float-cast', 'double-literal'):
        decisions = controls['tone'][label]['trace']['decisions']
        assert [d['narrowed'] for d in decisions] == (
            original_pattern if label == 'double-literal' else [False, False, True])
    counts = saved['counts']
    assert counts['above_size_bound'] == 0
    assert counts['emitted_bodies'] == sum(counts[k] for k in (
        'without_reference', 'exact_names_excluded', 'eligible_nonexact_copies'))
    assert counts['eligible_nonexact_copies'] == counts['no_original_float_compare']+counts['traced_copies']
    assert counts['traced_copies'] == counts['unequal_decision_counts']+counts['nominated_copies']+counts['no_local_narrowing_deficit']
    assert counts['nominated_copies'] == len(saved['nominations']) == 0
    original = screen.load(args.reference)
    # The initial screen nominated these eight original store/reload chains.
    # Independent operand review shows FABS on an immediately loaded SF
    # coefficient: finite input precision cannot increase at these sites.
    reviewed = {}
    for symbol, offsets in (
            ('_ZN12V90Equalizer11enterPhase4Ev', (428, 477, 1228, 1277)),
            ('_ZN12V90Equalizer14enterDataPhaseEv', (140, 189, 940, 989))):
        rows = original[symbol]
        base = rows[0].address
        chains = []
        for off in offsets:
            index = next(n for n, i in enumerate(rows) if i.address-base == off)
            load, unary, store, reload, compare = rows[index-4:index+1]
            assert load.mnemonic == 'fld' and load.operands[0].size == 4
            assert unary.mnemonic == 'fabs'
            assert store.mnemonic == 'fstp' and store.operands[0].size == 4
            assert reload.mnemonic == 'fld' and reload.operands[0].size == 4
            assert screen.slot(store, store.operands[0]) == screen.slot(reload, reload.operands[0])
            assert compare.mnemonic == 'fcomp'
            chains.append([{'at': i.address-base, 'instruction': i.mnemonic+' '+i.op_str}
                           for i in (load, unary, store, reload, compare)])
        reviewed[symbol] = {'classification': 'exact FABS of directly loaded SF; finite-value negative',
                            'original_chains': chains}
    survivors = {}
    for row in saved['traces']:
        a = [d for d in row['original']['decisions'] if d['narrowed']]
        b = [d for d in row['retained']['decisions'] if d['narrowed']]
        if a or b:
            assert len(a) == len(b) == 2
            assert row['symbol'] in ('TONE_generate', '_Z22V92DILdescriptorPackerP19tagV90DILdescriptorPhPi')
            widths = [4, 4] if row['symbol'] == 'TONE_generate' else [8, 8]
            for side in (a, b):
                assert [d['arithmetic_values'][0]['width'] for d in side] == widths
            survivors[row['symbol']] = {'original': a, 'retained': b,
                'correspondence': 'reviewed elapsed/phase pair or FRNDINT DIL ceiling pair; no missing local conversion'}
    assert len(survivors) == 2
    period = {}
    if args.period_dir:
        for test, expected in (
                ('t_tonetimer', 'PASS constructed TONE elapsed rounding boundary 1032 checks'),
                ('t_v90equnumeric', 'PASS constructed equalizer reciprocal-scale quantization 812 checks'),
                ('t_v92dilpack', 'PASS')):
            text = (args.period_dir/(test+'.run.log')).read_text()
            assert expected in text and 'FAIL' not in text
            period[test] = {'sha256': screen.sha(args.period_dir/test), 'run_log': text}
    result = {'screen_sha256': screen.sha(args.screen), 'counts': counts,
              'controls': '19 assembled cases; old tone/cast-positive, repaired tone-negative',
              'reviewed_exact_unary_negatives': reviewed, 'matching_local_narrowing_pairs': survivors,
              'unaligned': [dict(symbol=r['symbol'], original=len(r['original']['decisions']),
                                 retained=len(r['retained']['decisions'])) for r in saved['unaligned']],
              'focused_period': period,
              'conclusion': 'No new source discrepancy established in the supported local paths.',
              'limits': 'Four original narrowing decisions on two named bodies are locally matched. '
                        'Unknown block-entry provenance, nonstack aliases, calls, unsupported x87, '
                        'flag scheduling and eight unaligned bodies remain explicit. '
                        'No global floating-fidelity or modem-reachability claim.',
              'next_discriminator': 'Model proven flags-preserving compare-to-branch scheduling, '
                                    'then cross-block value provenance with positive/negative CFG controls.'}
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print('PASS 19 assembled controls; old tone positive/repaired negative; '
          '8 exact-unary negatives; 4 matched local narrowing decisions; '
          '%d unaligned bodies; %d focused period logs' % (len(saved['unaligned']), len(period)))


if __name__ == '__main__':
    main()
