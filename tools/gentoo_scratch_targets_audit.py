#!/usr/bin/env python3
"""Audit the four non-cloned target bodies and complete scratch replay objects."""
import argparse
import json
from pathlib import Path
from gentoo_peep2_search_audit import validate, sha
from gentoo_peep2_search_reproduce import ROOT, PINS
import playbook_small_patterns as d
from gcc3_stage_divergence import chunks, fingerprint
from gcc3_reload_trace import instructions

TARGETS = {
    'toneiir': ('_iir_filter_create', '_iir_filter_create', 'toneiir.c', 0),
    'FloatFIR': ('_ZN8FloatFIR5resetEv', 'void FloatFIR::reset()', 'FloatFIR.cpp', 1),
    'v22': ('dp_v22_init', 'dp_v22_init', 'v22.c', 3),
    'V27_SDM': ('SDMv27_init', 'SDMv27_init', 'V27_SDM.c', 0),
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=ROOT/'build/gentoo-scratch-target-traces')
    args = parser.parse_args()
    root = args.input.resolve()
    result = json.loads((root/'results.json').read_text())
    assert len(result['controls']) == 5 and not result['source_or_rtl_mutation']
    traces, rows, target_rows = {}, [], []
    for control in result['controls']:
        label = control['control']
        folder, saved = root/label, Path(control['source_directory'])
        events = json.loads((folder/'events.json').read_text())
        groups, matches = validate(events)
        traces[label] = events
        assert sha(root/control['compiler']) == PINS[control['compiler']]
        assert sha(ROOT/'tools/gentoo_peep2_search_trace.py') == control['observer_sha256']
        inputs = [p for p in saved.iterdir() if p.suffix in ('.i', '.ii')]
        assert len(inputs) == 1
        assert sha(folder/inputs[0].name) == control['preprocessed_sha256']
        assert len(groups) == control['searches']
        assert sum(len(g['candidates']) for g in groups.values()) == control['candidate_visits']
        assert all(m['replacement_returned'] for m in matches.values())
        for key, path in [('raw', folder/'raw.o'), ('traced', folder/'traced.o'), ('saved', saved/'candidate.o')]:
            assert sha(path) == control['complete_object_hashes'][key]
        assert len(set(control['complete_object_hashes'].values())) == 1
        bodies = {n: d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(folder/'raw.o'), n))
                  for n in sorted(set(d.b.sizes(str(folder/'raw.o')))&set(d.b.sizes(d.b.BLOB)))}
        rows.append({'control': label, 'searches': len(groups), 'body_verdicts': bodies})
        if label not in TARGETS:
            continue
        symbol, header, stem, count = TARGETS[label]
        assert symbol in bodies, 'missing target is not a zero-search result'
        assert d.b.alpha_equal(d.b.insns(d.b.BLOB, symbol), d.b.insns(str(folder/'raw.o'), symbol))
        target = [g for g in groups.values() if g['entry']['assembler_name'] == symbol]
        assert len(target) == count
        streams = {stage: instructions(chunks(saved/(stem+'.'+stage), header)[0])
                   for stage in ('27.flow2', '28.peephole2', '30.rnreg')}
        assert all(len(chunks(saved/(stem+'.'+stage), header)) == 1 for stage in streams)
        transitions = {}
        for before, after in [('27.flow2', '28.peephole2'), ('28.peephole2', '30.rnreg')]:
            a, b = streams[before], streams[after]
            changed = [uid for uid in sorted(set(a)|set(b))
                       if fingerprint(a.get(uid)) != fingerprint(b.get(uid))]
            transitions[before+' -> '+after] = changed
        target_rows.append({'symbol': symbol, 'searches': count, 'stage_changes': transitions,
                            'events': [{'entry': g['entry'], 'return': g['return']} for g in target]})
    assert traces['V27_SDM'] == traces['V27_SDM-repeat']
    output = {'controls': rows, 'targets': target_rows,
              'clone_attribution': 'targets have one RTL body; other clone declarations are not attributed to emitted clone symbols',
              'strict_gains': 0, 'source_changes': False}
    (root/'audit.json').write_text(json.dumps(output, indent=2)+'\n')
    print(len(rows), 'raw/traced/saved object triples;', sum(r['searches'] for r in rows),
          'searches;', sum(c['candidate_visits'] for c in result['controls']),
          'candidates; 1 independent repeat')
    for row in target_rows:
        print(row['symbol'], 'scratch searches:', row['searches'], row['stage_changes'])

if __name__ == '__main__':
    main()
