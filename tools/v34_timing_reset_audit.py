#!/usr/bin/env python3
"""Verify all timing-reset stores, raw controls and complete-TU collateral."""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc_x87_transfer_screen import assembly
from gcc3_value_carriers_audit import inspect
from value_timing_tail_screen import operands
from branch_store_paths import destination
from gentoo_peep2_search_audit import validate
from gentoo_peep2_search_reproduce import PINS, sha


def stores(path):
    constants = {}
    result = []
    for _, op, arg in assembly(path)['rxtiminginit']:
        args = operands(arg)
        if op == 'call':
            for reg in ('eax', 'ecx', 'edx'):
                constants.pop(reg, None)
        elif op == 'xor' and args[0] == args[-1] and args[-1].startswith('%'):
            constants[args[-1][1:]] = 0
        elif op.startswith('mov') and args[-1].startswith('%'):
            reg = args[-1][1:]
            if args[0].startswith('$'):
                constants[reg] = int(args[0][1:], 0)
            else:
                constants.pop(reg, None)
        elif op == 'mov' and destination(args[-1]) is not None:
            offset, base = destination(args[-1])
            if base != 'esi' or not args[0].startswith('%'):
                continue
            reg = args[0][1:]
            width = 2 if reg in ('ax', 'bx', 'cx', 'dx') else 4
            owner = 'e'+reg if width == 2 else reg
            if owner not in constants:
                continue
            result.append([offset, width, constants[owner] & ((1 << (width*8))-1)])
    return result


def main():
    root = d.ROOT/'build/v34-timing-reset'
    family = root/'V34RX'
    cells = json.loads((root/'results.json').read_text())['families']['V34RX']['cells']
    base = family/'baseline/candidate.o'
    assert base.read_bytes() == (family/'retained.o').read_bytes()
    old_metadata = inspect(base)
    assert len(d.b.sizes(str(base))) == 12
    collateral = {}
    for label, cell in cells.items():
        path = family/label/'candidate.o'
        assert cell['compile_exit'] == 0
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        assert '-DDSPLIB_REPRODUCE_BUGS' in cell['command'][-1]
        meta = inspect(path)
        assert all(meta[k] == v for k, v in old_metadata.items() if k != 'text_positions')
        assert cell['functions'] == cells['baseline']['functions']
        assert cell['globals'] == cells['baseline']['globals']
        assert not cell.get('gains', []) and not cell.get('losses', [])
        for n, verdict in cell['verdicts'].items():
            assert list(d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(path), n))) == verdict
        collateral[label] = cell.get('changed_bodies', [])
    original = stores(Path(d.b.BLOB))
    selected = stores(family/'order-1-halves-1/candidate.o')
    assert len(original) == 23 and original == selected
    assert stores(base) != original
    assert stores(family/'order-1-halves-0/candidate.o') != original
    assert stores(family/'order-0-halves-1/candidate.o') != original
    trace_root = root/'traces'
    replay = json.loads((trace_root/'results.json').read_text())
    trace_rows = []
    assert len(replay['controls']) == 2
    for control in replay['controls']:
        folder = trace_root/control['control']
        groups, matches = validate(json.loads((folder/'events.json').read_text()))
        assert sha(trace_root/control['compiler']) == PINS[control['compiler']]
        assert sha(d.ROOT/'tools/gentoo_peep2_search_trace.py') == control['observer_sha256']
        assert sha(folder/'V34RX.i') == control['preprocessed_sha256']
        for key, path in [('raw', folder/'raw.o'), ('traced', folder/'traced.o'),
                          ('saved', Path(control['source_directory'])/'candidate.o')]:
            assert sha(path) == control['complete_object_hashes'][key]
        assert len(set(control['complete_object_hashes'].values())) == 1
        assert all(m['replacement_returned'] for m in matches.values())
        target = [g for g in groups.values() if g['entry']['assembler_name'] == 'rxtiminginit']
        assert len(target) == (23 if control['control'] == 'baseline' else 24)
        assert target[0]['entry']['cursor_before'] == 50
        trace_rows.append({'control': control['control'], 'searches': len(groups),
                           'target_searches': len(target),
                           'candidates': sum(len(g['candidates']) for g in groups.values())})
    assert sum(r['searches'] for r in trace_rows) == 249
    assert sum(r['candidates'] for r in trace_rows) == 1620
    (root/'audit.json').write_text(json.dumps({'original_stores': original,
        'matched_selected_stores': len(selected), 'full_TU_controls': 4,
        'body_verdicts': 48, 'changed_bodies': collateral, 'scratch_traces': trace_rows,
        'gains': [], 'losses': [], 'negative_store_controls': 3}, indent=2)+'\n')
    print('23 original offset/width/value stores match; 3 negative controls differ')
    print('2 raw/traced/saved triples; 249 searches / 1620 visits; target23 -> 24')
    print('4 full-TU controls / 48 verdicts; bindings/nontext metadata pass; 0 exact gains/losses')


if __name__ == '__main__':
    main()
