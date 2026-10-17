#!/usr/bin/env python3
"""Expand selected structural nominations through direct same-TU calls/tails.

Presence/side-effect inventory only: no path multiplicity, indirect dispatch,
argument equivalence or proof of functional equivalence. Named PC32 CALL/JMP
forms distinguish sibling transfers from omitted callees. Reuses the existing
relocation-aware instruction inventory and call/tail graph parser.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

from gcc3_alignment_stack_screen import inventory
from recovery_large_debug_audit import graph
from structural_sideeffect_screen import signature
from small_call_result_screen import forms, compare as call_to_tail
import playbook_small_patterns as d


def expand(edges, root, allowed):
    seen, todo = set(), [root]
    while todo:
        node = todo.pop()
        if node in seen:
            continue
        seen.add(node)
        todo.extend(target for target in edges.get(node, ())
                    if target in allowed and target not in seen)
    return seen


def describe(rows, edges, root, allowed):
    nodes = expand(edges, root, allowed)
    stores = Counter()
    for name in nodes:
        stores.update(dict(signature(rows[name])['stores']))
    return {'bodies': sorted(nodes),
            'root_calls': rows[root]['calls'],
            'root_direct_call_or_tail_targets': sorted(edges.get(root, set()) - {root}),
            'external_direct_boundaries': sorted({target for name in nodes
                for target in edges.get(name, ()) if target not in allowed}),
            'indirect_calls': sum(sum(mn == 'call' and ops.startswith('*')
                for mn, ops in rows[name]['normalized']) for name in nodes),
            'physical_store_counts': sorted(stores.items())}


def controls():
    source = '.text\n.globl root\n.type root,@function\nroot: call helper_a\njmp helper_b\n.size root,.-root\n.type helper_a,@function\nhelper_a: movw $1,(%edx)\nret\n.size helper_a,.-helper_a\n.type helper_b,@function\nhelper_b: movw $2,2(%edx)\ncall external\nret\n.size helper_b,.-helper_b\n'
    with tempfile.TemporaryDirectory() as directory:
        results = {}
        transfer_forms = {}
        for name, text in {'base': source,
                           'missing_call': source.replace('call helper_a', 'nop'),
                           'missing_tail': source.replace('jmp helper_b', 'ret'),
                           'missing_store': source.replace('movw $1,(%edx)', 'nop'),
                           'external_tail': source.replace('call external', 'jmp external')}.items():
            path = Path(directory) / (name + '.s'); obj = path.with_suffix('.o')
            path.write_text(text)
            subprocess.run(['as', '--32', '-o', str(obj), str(path)], check=True)
            rows = inventory(obj)
            transfer_forms[name] = forms(obj, 'helper_b')
            results[name] = describe(rows, graph(obj, True), 'root', set(rows))
        assert results['base']['bodies'] == ['helper_a', 'helper_b', 'root']
        assert results['base']['external_direct_boundaries'] == ['external']
        assert results['missing_call']['bodies'] == ['helper_b', 'root']
        assert results['missing_tail']['bodies'] == ['helper_a', 'root']
        assert not results['missing_tail']['external_direct_boundaries']
        assert dict(results['base']['physical_store_counts'])['movw'] == 2
        assert dict(results['missing_store']['physical_store_counts'])['movw'] == 1
        assert len(call_to_tail(transfer_forms['base'], transfer_forms['external_tail'])) == 1
        assert call_to_tail(transfer_forms['base'], transfer_forms['external_tail'])[0][1] == 'external'
        assert not call_to_tail(transfer_forms['base'], transfer_forms['base'])
    return {'same_tu_call_expansion': True, 'sibling_tail_expansion': True,
            'external_boundary_retained': True, 'removed_call_rejected': True,
            'removed_tail_rejected': True, 'removed_callee_store_rejected': True,
            'named_call_to_tail_positive': True, 'unchanged_transfer_negative': True,
            'scope': 'actual assembled synthetic i386 ELF detector controls, not modem lifecycle'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path)
    parser.add_argument('--screen', type=Path)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--symbol', action='append', default=[])
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    checked = controls()
    if args.self_test:
        print(json.dumps(checked, indent=2)); return
    if args.objects is None or args.screen is None or args.out is None or not args.symbol:
        parser.error('--objects, --screen, --out and at least one --symbol are required')
    screen = json.loads(args.screen.read_text())
    original = inventory(Path(d.b.BLOB)); old_edges = graph(Path(d.b.BLOB), True)
    report = {'controls': checked, 'screen_sha256': hashlib.sha256(args.screen.read_bytes()).hexdigest(),
              'selected_roots': len(args.symbol), 'rows': [],
              'limits': 'Selected roots only. Expand only names defined in the candidate TU and original. External-TU direct calls and all indirect calls remain boundaries. Physical stores are not dynamic execution counts. Callee presence does not prove argument/timing/path equivalence.'}
    for symbol in args.symbol:
        matches = [r for r in screen['rows'] if r['symbol'] == symbol]
        if len(matches) != 1:
            raise ValueError(('symbol must identify exactly one nominated emitted copy', symbol))
        row = matches[0]; obj = args.objects / row['object']
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == screen['objects'][row['object']]
        own = inventory(obj); edges = graph(obj, True)
        allowed = set(own) & set(original)
        before_forms = forms(Path(d.b.BLOB), symbol)
        after_forms = forms(obj, symbol)
        transfers = lambda pair: [{'target': list(target), 'calls': pair[0][target], 'tails': pair[1][target]}
                                  for target in sorted(set(pair[0]) | set(pair[1]))]
        report['rows'].append({'original_named_transfers': transfers(before_forms),
            'compiled_named_transfers': transfers(after_forms),
            'call_to_tail_only': call_to_tail(before_forms, after_forms), 'symbol': symbol, 'source': row['source'], 'object': row['object'],
            'original': describe(original, old_edges, symbol, allowed),
            'compiled': describe(own, edges, symbol, allowed)})
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n')
    print(len(report['rows']), 'selected roots;', len(checked)-1, 'assembled graph controls pass; indirect/external boundaries remain')


if __name__ == '__main__':
    main()
