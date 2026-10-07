#!/usr/bin/env python3
"""Prove original countdown boundaries, preserved work and complete-TU controls."""
import json
from pathlib import Path
from gentoo_peep2_search_reproduce import ROOT, PINS
from gentoo_peep2_search_audit import validate, sha
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

SYMBOL = '_ZN8FloatFIR7processEPKfPfj'

def sentinels(rows):
    return [a for (op, a), (next_op, next_a) in zip(rows, rows[1:])
            if op == 'dec' and next_op == 'cmp' and next_a == '$0xffffffff,'+a]

def main():
    root = ROOT/'build/floatfir-consumed-count/FloatFIR'
    before, after = root/'baseline/candidate.o', root/'consumed-entry-count/candidate.o'
    rows = {key: d.b.insns(str(path), SYMBOL) for key, path in
            [('original', d.b.BLOB), ('baseline', before), ('candidate', after)]}
    assert len(sentinels(rows['original'])) == len(sentinels(rows['candidate'])) == 2
    assert len(set(sentinels(rows['candidate']))) == 1
    assert not sentinels(rows['baseline']), 'negative control should reject baseline'
    def work(stream):
        return [(op, args) for op, args in stream if not op.startswith('j')
                and not d.b._padding(op, args)
                and (op, args) not in [('dec', '%ebp'), ('test', '%ebp,%ebp'),
                                      ('cmp', '$0xffffffff,%ebp')]]
    assert work(rows['baseline']) == work(rows['candidate'])
    a, b = inspect(before), inspect(after)
    assert a == b, 'bindings, positions, nontext data or relocation change'
    functions = d.b.sizes(str(before))
    assert functions == d.b.sizes(str(after)) and len(functions) == 8
    changed = [n for n in functions if d.b.body(str(before), n) != d.b.body(str(after), n)]
    assert changed == [SYMBOL]
    source = (ROOT/'src/dsp/FloatFIR.cpp').read_text()
    start = source.index('FloatFIR::process(const float *in, float *out, unsigned int count)')
    end = source.index('\n}\n', start)+2
    body = source[start:end]
    assert body.count('if (count-- == 0)') == body.count('while (count-- != 0)') == 1
    assert body.index('return;') < body.index('h = history;')
    trace_root = ROOT/'build/floatfir-consumed-count-traces'
    replay = json.loads((trace_root/'results.json').read_text())
    assert len(replay['controls']) == 2
    histories = []
    for control in replay['controls']:
        folder = trace_root/control['control']
        groups, matches = validate(json.loads((folder/'events.json').read_text()))
        assert len(groups) == 7 and all(m['replacement_returned'] for m in matches.values())
        assert sha(trace_root/control['compiler']) == PINS[control['compiler']]
        assert sha(ROOT/'tools/gentoo_peep2_search_trace.py') == control['observer_sha256']
        assert sha(folder/'FloatFIR.ii') == control['preprocessed_sha256']
        for key, path in [('raw', folder/'raw.o'), ('traced', folder/'traced.o'),
                          ('saved', Path(control['source_directory'])/'candidate.o')]:
            assert sha(path) == control['complete_object_hashes'][key]
        assert len(set(control['complete_object_hashes'].values())) == 1
        histories.append([(g['entry']['assembler_name'], g['entry']['generator'],
                           g['entry']['cursor_before'], g['return']['selected'],
                           g['return']['cursor_after']) for g in groups.values()])
    assert histories[0] == histories[1], 'count recovery unexpectedly changes scratch history'
    out = {'body_verdicts': {n: d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(after), n)) for n in functions},
           'sentinel_pairs': {key: sentinels(stream) for key, stream in rows.items()},
           'unchanged_nonbranch_noncounter_work': len(work(rows['candidate'])),
           'unchanged_bystanders': 7, 'baseline_refused': True, 'strict_gains': 0,
           'unchanged_scratch_history': histories[0], 'observed_searches': 14}
    (root.parent/'audit.json').write_text(json.dumps(out, indent=2)+'\n')
    print('original/candidate: 2 sentinel boundaries; baseline refused; 7/7 bystanders unchanged; 0 exact gains')

if __name__ == '__main__':
    main()
