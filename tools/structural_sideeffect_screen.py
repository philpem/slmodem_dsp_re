#!/usr/bin/env python3
"""Bounded object triage for loop operands and side-effect deficits, never proof.

Reuses the established relocation-aware instruction inventory. Counts and loop
signatures nominate manual review; unrolling, induction changes, inlining and
shared tails can explain every difference. No source or object is modified.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

from gcc3_alignment_stack_screen import inventory
import playbook_small_patterns as d


def signature(row):
    stream = row['normalized']
    loops = []
    for end, (mn, ops) in enumerate(stream):
        target = re.match(r'insn:(\d+)\b', ops)
        if not target or not mn.startswith('j') or mn == 'jmp':
            continue
        start = int(target[1])
        if start >= end:
            continue
        region = stream[start:end + 1]
        bounds = Counter()
        scales = Counter()
        for op, args in region:
            if op.startswith(('cmp', 'test')):
                bounds.update(re.findall(r'\$(0x[0-9a-f]+)\b', args))
            scales.update(re.findall(r'\([^)]*,([1248])\)', args))
        loops.append({'start': start, 'end': end, 'branch': mn,
                      'bounds': sorted(bounds.items()), 'scales': sorted(scales.items())})
    stores = Counter()
    for mn, ops in stream:
        if not mn.startswith(('mov', 'fst', 'fist', 'stos')):
            continue
        # AT&T final operand, with parentheses protected from comma splitting.
        dest = re.split(r',(?=[^()]*$)', ops)[-1]
        if ('(' in dest or '@(' in dest) and '%esp' not in dest and '%ebp' not in dest:
            stores[mn] += 1
    return {'loops': loops, 'stores': sorted(stores.items()), 'calls': len(row['calls'])}


def flattened(sig, key):
    return Counter(item for loop in sig['loops'] for item, n in loop[key] for _ in range(n))


def differences(left, right):
    a, b = signature(left), signature(right)
    return {'loop_count': [len(a['loops']), len(b['loops'])],
            'bounds_changed': flattened(a, 'bounds') != flattened(b, 'bounds'),
            'scales_changed': flattened(a, 'scales') != flattened(b, 'scales'),
            'branches_changed': Counter(l['branch'] for l in a['loops']) != Counter(l['branch'] for l in b['loops']),
            'call_deficit': a['calls'] - b['calls'],
            'store_deficit': sum(dict(a['stores']).values()) - sum(dict(b['stores']).values()),
            'original': a, 'compiled': b}


def controls():
    # Deliberately semantic assembly changes: write five rather than four
    # elements, advance four rather than two bytes, and remove a store/call.
    base = '.text\n.type probe,@function\nprobe:\nxor %eax,%eax\n1: movw $1,(%edx,%eax,2)\ninc %eax\ncmp $4,%eax\njl 1b\ncall external\nret\n.size probe,.-probe\n'
    variants = {'base': base, 'bound': base.replace('$4', '$5'),
                'stride': base.replace('%eax,2', '%eax,4'),
                'store': base.replace('movw $1,(%edx,%eax,2)', 'nop'),
                'branch': base.replace('jl 1b', 'jg 1b'),
                'call': base.replace('call external', 'nop')}
    with tempfile.TemporaryDirectory() as directory:
        rows = {}
        for name, text in variants.items():
            source = Path(directory) / (name + '.s'); obj = source.with_suffix('.o')
            source.write_text(text)
            subprocess.run(['as', '--32', '-o', str(obj), str(source)], check=True)
            rows[name] = inventory(obj)['probe']
        assert not differences(rows['base'], rows['base'])['bounds_changed']
        assert differences(rows['base'], rows['bound'])['bounds_changed']
        assert differences(rows['base'], rows['stride'])['scales_changed']
        assert differences(rows['base'], rows['store'])['store_deficit'] == 1
        assert differences(rows['base'], rows['call'])['call_deficit'] == 1
        assert differences(rows['base'], rows['branch'])['branches_changed']
    return {'identical_negative': True, 'changed_bound': True, 'changed_stride': True,
            'removed_store': True, 'removed_call': True, 'changed_condition': True,
            'boundary': 'assembled synthetic i386 detector controls; not reconstructed modem inputs'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--max-bytes', type=int, default=700)
    args = parser.parse_args()
    if args.self_test:
        print(json.dumps(controls(), indent=2))
        return
    if args.objects is None or args.out is None:
        parser.error('--objects and --out are required unless --self-test is used')
    report = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=d.ROOT, text=True).strip(),
              'blob_sha256': hashlib.sha256(Path(d.b.BLOB).read_bytes()).hexdigest(),
              'objects_directory': str(args.objects.resolve()),
              'controls': controls(), 'counts': Counter(), 'objects': {}, 'rows': [],
              'max_original_bytes': args.max_bytes, 'config': (args.objects / '.build-config').read_text(),
              'limits': 'Supplied frozen objects are read without recompilation; revision describes this checkout, not independently inferred object provenance. Per-emitted-copy screen. Physical conditional backedges only; no switch/irreducible-loop analysis. Bounds/scales are multisets, not SSA induction proofs. Store heuristic excludes esp/ebp and read-modify-write instructions. Deficits are not missing-behavior claims. Identical normalized instruction streams excluded; no claim of semantic equivalence or global CFG coverage.'}
    assert '-DDSPLIB_REPRODUCE_BUGS' in report['config']
    original = inventory(Path(d.b.BLOB))
    for line in (args.objects / 'tc_manifest.txt').read_text().splitlines():
        name, source = line.split(); path = args.objects / name
        report['counts']['objects'] += 1
        report['objects'][name] = hashlib.sha256(path.read_bytes()).hexdigest()
        for symbol, row in inventory(path).items():
            report['counts']['emitted_copies'] += 1
            if symbol not in original: continue
            report['counts']['original_mapped_copies'] += 1
            left = original[symbol]
            if left['normalized'] == row['normalized']:
                report['counts']['identical_normalized_streams'] += 1
                continue
            report['counts']['different_normalized_streams'] += 1
            if left['size'] > args.max_bytes: continue
            report['counts']['bounded_nonidentical_copies'] += 1
            delta = differences(left, row)
            if not delta['original']['loops']: continue
            report['counts']['bounded_original_backedge_copies'] += 1
            if not (delta['bounds_changed'] or delta['scales_changed'] or delta['branches_changed'] or delta['call_deficit'] > 0 or delta['store_deficit'] > 0): continue
            report['rows'].append({'symbol': symbol, 'source': source, 'object': name,
                                   'sizes': [left['size'], row['size']], **delta})
    report['counts']['nominated_copies'] = len(report['rows'])
    assert report['counts']['objects'] and report['counts']['bounded_original_backedge_copies']
    report['rows'].sort(key=lambda r: (r['loop_count'][0] != r['loop_count'][1],
        not (r['bounds_changed'] and not r['scales_changed']), r['sizes'][0], r['symbol']))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n')
    print(dict(report['counts']))
    print('6 assembled detector controls pass;', len(report['rows']), 'nominations; manual review required')


if __name__ == '__main__':
    main()
