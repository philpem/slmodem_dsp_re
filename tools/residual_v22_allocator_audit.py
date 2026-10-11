#!/usr/bin/env python3
"""Validate V22 allocation equality and the later scratch-history difference."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gentoo_peep2_search_audit import validate
from gentoo_peep2_search_reproduce import PINS, sha


def main():
    out = d.ROOT / 'build/residual-v22-allocator'
    saved = json.loads((out / 'results.json').read_text())
    assert sha(out / 'cc1') == saved['compiler_hash'] == PINS['cc1']
    assert sha(d.ROOT / 'tools/residual_v22_allocator_gdb.py') == saved['observer_hash']
    assert [row['label'] for row in saved['cells']] == ['baseline', 'combined', 'baseline-repeat']
    streams = []
    for row in saved['cells']:
        folder = out / row['label']
        assert sha(folder / 'plain.o') == sha(folder / 'traced.o') == row['object_hash']
        assert sha(folder / 'v22prc.i') == row['input_hash']
        actual = json.loads((folder / 'observe.json').read_text())
        assert not actual['errors'] and actual['exits'] == [0]
        assert actual['events'] == row['events']
        events = row['events']
        assert [e['stage'] for e in events if e['stage'] != 'reload-choice'] == ['global-entry', 'reload-entry', 'reload-return']
        assert sum(e['stage'] == 'reload-choice' for e in events) == 15
        streams.append(events)
    assert streams[0] == streams[1] == streams[2]
    root = d.ROOT / 'build/residual-v22-scratch'
    scratch = json.loads((root / 'results.json').read_text())
    assert len(scratch['controls']) == 3 and not scratch['source_or_rtl_mutation']
    searches = visits = 0
    groups = {}
    repeats = {}
    for row in scratch['controls']:
        folder = root / row['control']
        assert sha(root / row['compiler']) == PINS[row['compiler']] == row['compiler_sha256']
        assert sha(d.ROOT / 'tools/gentoo_peep2_search_trace.py') == row['observer_sha256']
        assert sha(folder / 'raw.o') == sha(folder / 'traced.o') == sha(Path(row['source_directory']) / 'candidate.o')
        events = json.loads((folder / 'events.json').read_text())
        repeats[row['control']] = events
        found, _ = validate(events)
        groups[row['control']] = list(found.values())
        searches += len(found)
        visits += sum(len(g['candidates']) for g in found.values())
    assert repeats['baseline'] == repeats['baseline-repeat']
    assert groups['baseline'][:3] == groups['combined'][:3]
    target = {}
    for label, found in groups.items():
        target[label] = [(g['entry']['uid'], g['entry']['cursor_before'],
                          g['return']['selected'], g['return']['cursor_after'])
                         for g in found if g['entry']['assembler_name'] == 'Detect_1s']
    assert target['baseline'] == [(94, 50, 1, 2), (33, 2, 2, 3)]
    assert target['combined'] == [(94, 13, 4, 14), (33, 14, 0, 49)]
    assert searches == 46 and visits == 283
    report = {'raw_boundary_controls': 3, 'boundary_snapshots': 9,
              'equal_reload_choices': 45, 'scratch_raw_controls': 3,
              'scratch_searches': searches, 'scratch_visits': visits,
              'scratch_target': target, 'source_adopted': False}
    (out / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('3 raw boundary controls / 9 snapshots / 45 equal reload choices')
    print('3 raw scratch controls / 46 validated searches / 283 candidate visits')


if __name__ == '__main__':
    main()
