#!/usr/bin/env python3
"""Validate scratch-search traces against observed availability and cursor state."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
from gentoo_peep2_search_reproduce import PINS, ROOT

def mask(words):
    assert len(words) == 2
    return words[0] | (words[1] << 32)

def validate(events):
    assert events[0]['kind'] == 'observer'
    assert events[0]['compiler_sha256'] in PINS.values()
    assert not events[0]['source_or_rtl_mutation'] and not events[0]['inferior_function_calls']
    orders = [e['registers'] for e in events if e['kind'] == 'allocation_order']
    assert len(orders) == 1 and len(orders[0]) == 53
    order = orders[0]
    groups, matches = {}, {}
    previous = 0
    current = None
    for e in events[1:]:
        kind = e['kind']
        if kind == 'allocation_order':
            continue
        if kind == 'enter':
            assert current is None and e['search'] == len(groups)+1
            assert e['cursor_before'] == previous
            assert e['mode'] in (11, 12) and e['constraint'] == 'r', 'outside validated HI/SI general-register domain'
            current = {'entry': e, 'candidates': []}
            groups[e['search']] = current
        elif kind == 'candidate':
            assert current is not None and current['entry']['search'] == e['search']
            i = len(current['candidates'])
            assert e['raw_reg'] == (previous+i) % 53
            assert e['reg'] == order[e['raw_reg']]
            current['candidates'].append(dict(e))
        elif kind == 'mode_check':
            assert current is not None and current['entry']['search'] == e['search']
            candidate = current['candidates'][-1]
            assert candidate['reg'] == e['reg'] and 'mode_allowed' not in candidate
            candidate['mode_allowed'] = bool(e['allowed'])
        elif kind == 'return':
            assert current is not None and current['entry']['search'] == e['search']
            available = []
            for c in current['candidates']:
                reg = c['reg']
                if (not c['fixed'] and mask(c['class_contents']) & (1 << reg)
                    and c.get('mode_allowed', False)
                    and (c['call_used'] or c['ever_live']) and not c['frame_protected']
                    and not ((mask(c['live']) | mask(c['reserved'])) & (1 << reg))):
                    assert 0 <= reg < 8, 'outside single-GPR HI/SI model'
                    available.append(c)
            chosen = available[0] if available else None
            assert e['selected'] == (chosen['reg'] if chosen else None)
            assert len(available) <= 1 and (chosen is None or chosen is current['candidates'][-1])
            expected_cursor = (chosen['raw_reg']+1) % 53 if chosen else 0
            assert e['cursor_after'] == expected_cursor
            before = mask(current['entry']['reserved_before'])
            assert mask(e['reserved_after']) == before | ((1 << chosen['reg']) if chosen else 0)
            current['return'] = e
            previous, current = e['cursor_after'], None
        elif kind == 'match_return':
            assert e['match'] not in matches
            assert e['searches'] and all(i in groups and 'return' in groups[i] for i in e['searches'])
            assert all(groups[i]['entry']['match'] == e['match'] for i in e['searches'])
            assert e['cursor_after'] == groups[e['searches'][-1]]['return']['cursor_after']
            matches[e['match']] = e
        else:
            raise AssertionError('unknown event '+kind)
    assert current is None and groups and len(matches) == len({g['entry']['match'] for g in groups.values()})
    assert sorted(i for m in matches.values() for i in m['searches']) == sorted(groups)
    return groups, matches

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=ROOT/'build/gentoo-peep2-search-witnesses')
    args = parser.parse_args()
    root = args.input.resolve()
    saved = json.loads((root/'results.json').read_text())
    assert len(saved['controls']) == 6 and not saved['source_or_rtl_mutation']
    rows, traces, grouped = [], {}, {}
    for control in saved['controls']:
        name = control['control']
        events = json.loads((root/name/'events.json').read_text())
        groups, matches = validate(events)
        assert len(groups) == control['searches']
        assert sum(len(g['candidates']) for g in groups.values()) == control['candidate_visits']
        assert sha(root/control['compiler']) == PINS[control['compiler']] == control['compiler_sha256']
        assert sha(ROOT/'tools/gentoo_peep2_search_trace.py') == control['observer_sha256']
        for label in ('raw', 'traced'):
            assert sha(root/name/(label+'.o')) == control['complete_object_hashes'][label]
        assert sha(Path(control['source_directory'])/'candidate.o') == control['complete_object_hashes']['saved']
        assert len(set(control['complete_object_hashes'].values())) == 1
        traces[name], grouped[name] = events, groups
        rows.append({'control': name, 'searches_predicted': len(groups),
                     'candidate_visits': sum(len(g['candidates']) for g in groups.values()),
                     'searches_returning_null': sum(g['return']['selected'] is None for g in groups.values()),
                     'enclosing_matches_rejected': sum(not m['replacement_returned'] for m in matches.values()),
                     'complete_raw_traced_saved_object_equal': True})
    for first, repeat in [('fax-retained', 'fax-retained-repeat'), ('ctor-pre', 'ctor-pre-repeat')]:
        assert traces[first] == traces[repeat], 'equal replay event streams differ'
    witnesses = []
    for left, right, symbol, uid in [
        ('fax-retained', 'fax-split-tail', 'EpochDetectV29', 14),
        ('ctor-pre', 'ctor-post', '_ZN13V90ParametersC2EP19_tagModemParameters', 32),
    ]:
        pair = []
        pair_groups = []
        for name in (left, right):
            selected = [g for g in grouped[name].values() if g['entry']['assembler_name'] == symbol and g['entry']['uid'] == uid]
            assert len(selected) == 1
            g = selected[0]
            pair_groups.append(g)
            pair.append({'control': name, 'cursor_before': g['entry']['cursor_before'],
                         'selected': g['return']['selected'], 'cursor_after': g['return']['cursor_after']})
        assert pair[0]['selected'] == 2 and pair[1]['selected'] == 0
        assert pair_groups[0]['entry']['input_pattern'] == pair_groups[1]['entry']['input_pattern']
        for key in ('live', 'reserved', 'class_contents'):
            assert pair_groups[0]['candidates'][0][key] == pair_groups[1]['candidates'][0][key]
        witnesses.append({'symbol': symbol, 'uid': uid, 'pair': pair})
    writer = '_ZN13V90Parameters12setToDefaultEv'
    before = [g['entry'] for g in grouped['ctor-pre'].values() if g['entry']['assembler_name'] == writer]
    after = [g['entry'] for g in grouped['ctor-post'].values() if g['entry']['assembler_name'] == writer]
    assert len(before) == 158 and len(after) == 157
    missing = [e for e in before if e['uid'] == 1246]
    assert len(missing) == 1
    pat = missing[0]['input_pattern']
    assert pat['code'] == 'SET' and pat['operands'][0]['mode'] == 'SImode'
    address = pat['operands'][0]['operands'][0]
    assert address['code'] == 'PLUS' and address['operands'][1]['value'] == 1076
    assert pat['operands'][1]['value'] == 1132068864
    assert [e['uid']+(1 if e['uid'] > 1246 else 0) for e in before if e['uid'] != 1246] == [e['uid'] for e in after]
    refused = []
    for label in ('wrong-cursor', 'wrong-register', 'missing-return', 'missing-match-return'):
        bad = copy.deepcopy(traces['fax-retained'])
        if label == 'wrong-cursor':
            next(e for e in bad if e['kind'] == 'enter')['cursor_before'] += 1
        elif label == 'wrong-register':
            next(e for e in bad if e['kind'] == 'return')['selected'] = 0
        else:
            kind = label[len('missing-'):].replace('-', '_')
            index = next(i for i,e in enumerate(bad) if e['kind'] == kind)
            del bad[index]
        try:
            validate(bad)
        except AssertionError:
            refused.append(label)
        else:
            raise AssertionError('negative control accepted: '+label)
    report = {'controls': rows, 'searches_predicted': sum(r['searches_predicted'] for r in rows),
              'candidate_visits': sum(r['candidate_visits'] for r in rows),
              'dynamic_positive_witnesses': witnesses, 'equal_replays': 2,
              'writer_missing_search': missing[0],
              'refusal_controls': refused,
              'limit': 'HI/SI general-register observed searches; no failed scratch search or rejected enclosing match observed'}
    (root/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('%d searches / %d candidate visits validated; 2 positive pairs, 2 equal repeats, %d refusal controls; 6 full-object triples equal' %
          (report['searches_predicted'], report['candidate_visits'], len(refused)))

if __name__ == '__main__':
    main()
