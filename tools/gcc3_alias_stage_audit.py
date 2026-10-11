#!/usr/bin/env python3
"""Check a bijection of MEM alias-set IDs before comparing RTL stages.

Never normalizes registers, memory addresses, offsets, sizes, alignments or
symbol names. Alias equivalence is proved across the complete paired stream.
"""
import copy
import json
import re
from pathlib import Path
from gcc3_stage_divergence import chunks, fingerprint
from gcc3_reload_trace import instructions

ROOT = Path(__file__).resolve().parents[1]


def aliases(node, path=()):
    result = {}
    if isinstance(node, list):
        if node and isinstance(node[0], str) and node[0].startswith('mem'):
            for i, token in enumerate(node[2:], 2):
                if isinstance(token, str) and re.fullmatch(r'\[\d+', token):
                    result[path + (i,)] = int(token[1:])
                    break
        for i, child in enumerate(node):
            if isinstance(child, list):
                result.update(aliases(child, path + (i,)))
    return result


def mapped(node, mapping):
    result = copy.deepcopy(node)
    for path, identity in aliases(node).items():
        cursor = result
        for i in path[:-1]:
            cursor = cursor[i]
        cursor[path[-1]] = '[' + str(mapping[identity])
    return result


def compare(left, right):
    forward, reverse = {}, {}
    occurrences = 0
    for uid in left.keys() & right.keys():
        a, b = aliases(left[uid]), aliases(right[uid])
        assert a.keys() == b.keys(), 'different MEM annotation structure'
        for path in a:
            old, new = a[path], b[path]
            assert forward.setdefault(old, new) == new, 'alias relation split'
            assert reverse.setdefault(new, old) == old, 'alias relation merged'
            assert (old == 0) == (new == 0), 'universal alias class changed'
            occurrences += 1
    differing = sorted(uid for uid in left.keys() | right.keys()
                       if uid not in left or uid not in right
                       or fingerprint(mapped(left[uid], forward)) != fingerprint(right[uid]))
    return {'alias_bijection': forward, 'alias_occurrences': occurrences,
            'differing_uids': differing}


def controls():
    def mem(alias, offset='4'):
        return ['set', ['reg:SI', '0'], ['mem:SI', ['plus:SI', ['reg:SI', '7'],
                ['const_int', offset]], '[' + str(alias), 'S4', 'A32]']]
    assert not compare({1: mem(13)}, {1: mem(12)})['differing_uids']
    assert compare({1: mem(13)}, {1: mem(12, '8')})['differing_uids'] == [1]
    changed = mem(12)
    changed[1][1] = '1'
    assert compare({1: mem(13)}, {1: changed})['differing_uids'] == [1]
    for left, right in (({1: mem(13), 2: mem(14)}, {1: mem(12), 2: mem(12)}),
                        ({1: mem(13), 2: mem(13)}, {1: mem(12), 2: mem(14)}),
                        ({1: mem(0)}, {1: mem(12)})):
        try:
            compare(left, right)
        except AssertionError:
            pass
        else:
            raise AssertionError('changed alias relationship accepted')
    return 6


def main():
    root = ROOT / 'build/residual-v22-ack/v22prc'
    rows = {}
    for stage in ('01.rtl', '24.lreg', '25.greg', '27.flow2', '28.peephole2', '30.rnreg'):
        left = instructions(chunks(root / 'baseline' / ('v22prc.c.' + stage), 'Detect_1s')[0])
        right = instructions(chunks(root / 'square-1-late-1' / ('v22prc.c.' + stage), 'Detect_1s')[0])
        row = compare(left, right)
        row['raw_differing_uids'] = sorted(uid for uid in left.keys() | right.keys()
                                         if fingerprint(left.get(uid)) != fingerprint(right.get(uid)))
        rows[stage] = row
        if stage < '28.peephole2':
            assert not row['differing_uids']
    assert rows['28.peephole2']['differing_uids'] == [198, 199, 201, 202]
    assert len(rows['30.rnreg']['differing_uids']) == 7
    report = {'controls': controls(), 'target': 'Detect_1s', 'stages': rows}
    (ROOT / 'build/residual-v22-alias-stage-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('6 positive/refusal controls; 6 stages; alias relationships preserved')
    for stage, row in rows.items():
        print(stage, 'raw', len(row['raw_differing_uids']), 'after checked bijection', row['differing_uids'])


if __name__ == '__main__':
    main()
