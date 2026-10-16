#!/usr/bin/env python3
"""Screen equal known-call sequences for first-use extension-mode differences.

No source family or size filter is applied. A sequence aligns call ordinals;
it does not establish equal arguments, CFG paths, aliases or an original ABI.
Branches without proved edge ownership and non-extension carrier uses refuse.
"""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path

import playbook_small_patterns as d
from narrow_call_boundary_screen import bodies
from recovery_sre_boundary_audit import first_return_use, self_test as flow_controls


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def compare(left, right, left_calls, right_calls):
    a = [site for site in left_calls if 'target' in site]
    b = [site for site in right_calls if 'target' in site]
    if [site['target'] for site in a] != [site['target'] for site in b]:
        return {'sequence_equal': False, 'pairs': [], 'nominations': []}
    pairs, nominations = [], []
    for ordinal, (original, retained) in enumerate(zip(a, b)):
        before = first_return_use(left, original['address'])
        after = first_return_use(right, retained['address'])
        entry = {'ordinal': ordinal, 'target': original['target'],
                 'original_call': original, 'retained_call': retained,
                 'original_use': before, 'retained_use': after}
        pairs.append(entry)
        if before['refusals'] or after['refusals']:
            continue
        original_modes = sorted({v['kind'] for v in before['outcomes']})
        retained_modes = sorted({v['kind'] for v in after['outcomes']})
        if original_modes != retained_modes:
            nominations.append(entry)
    return {'sequence_equal': True, 'pairs': pairs, 'nominations': nominations}


def self_test():
    flow_controls()
    def rows(mnemonic):
        return [{'address': '0', 'mnemonic': 'call', 'operands': 'callee'},
                {'address': '5', 'mnemonic': mnemonic,
                 'operands': '%al,%eax' if mnemonic in ('movsbl', 'movzbl') else '%ax,%eax'}]
    calls = [{'address': '0', 'target': 'callee', 'kind': 'relocated'}]
    signed, unsigned = rows('movswl'), rows('movzwl')
    assert len(compare(signed, unsigned, calls, calls)['nominations']) == 1
    assert not compare(signed, signed, calls, calls)['nominations']
    assert len(compare(rows('movsbl'), rows('movzbl'), calls, calls)['nominations']) == 1
    cwtl = rows('cwtl'); cwtl[1]['operands'] = ''
    assert not compare(cwtl, signed, calls, calls)['nominations']
    assert not compare(signed, unsigned, calls,
                       [dict(calls[0], target='other')])['sequence_equal']
    overwritten = rows('mov'); overwritten[1]['operands'] = '$0,%eax'
    assert not compare(signed, overwritten, calls, calls)['nominations']
    print('return-mode controls: 6 passed; word/byte mode positives, '
          'identity/equivalent-sign/mismatched-target/overwrite negatives')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--objects', type=Path)
    ap.add_argument('--reference', type=Path, default=Path(d.b.BLOB))
    ap.add_argument('--baseline-manifest', type=Path)
    ap.add_argument('--ledger-tool', type=Path, default=d.ROOT/'tools/recovery_residual_ledger.py')
    ap.add_argument('--h1-controls-dir', type=Path)
    ap.add_argument('--output', type=Path)
    ap.add_argument('--self-test', action='store_true')
    args = ap.parse_args()
    self_test()
    if args.self_test:
        return
    if any(v is None for v in (args.objects, args.baseline_manifest,
                               args.h1_controls_dir, args.output)):
        ap.error('--objects, --baseline-manifest, --h1-controls-dir and --output required')
    spec = importlib.util.spec_from_file_location('return_mode_ledger', args.ledger_tool)
    ledger = importlib.util.module_from_spec(spec); spec.loader.exec_module(ledger)
    snapshot = json.loads(args.baseline_manifest.read_text())
    expected = snapshot['objects']
    hashes = {path.name: sha(path) for path in args.objects.glob('*.o')}
    if hashes != expected or sha(args.reference) != snapshot['reference_sha256']:
        raise ValueError('screen objects/reference disagree with pinned baseline snapshot')
    config = (args.objects/'.build-config').read_text()
    if config != snapshot['config'] or '-DDSPLIB_REPRODUCE_BUGS' not in config:
        raise ValueError('baseline configuration mismatch or bug reproduction absent')
    reference = bodies(args.reference)
    sizes = d.b.sizes(str(args.reference))
    reference_calls = ledger.call_sites(args.reference, reference)
    real_controls = []
    for family, name in [('V17r_int', 'DemodDataV17'), ('V27r_int', 'DemodDataV27'),
                         ('V29r_int', 'DemodDataV29')]:
        for cell, positive in [('baseline', True), ('int-read-boundary', False)]:
            path = args.h1_controls_dir/family/cell/'candidate.o'
            parsed = bodies(path)
            measured = compare(reference[name], parsed[name], reference_calls[name],
                               ledger.call_sites(path, parsed)[name])
            matches = [v for v in measured['nominations'] if v['target'] == 'FPM_MRF_filter']
            assert measured['sequence_equal'] and bool(matches) == positive, (family, cell)
            real_controls.append({'symbol': name, 'cell': cell, 'positive': positive,
                                  'object_sha256': sha(path), 'nominations': len(matches)})
    counts, refusals, nominations = Counter(), Counter(), []
    seen = set()
    for line in (args.objects/'tc_manifest.txt').read_text().splitlines():
        object_name, source = line.split()
        path = args.objects/object_name
        parsed = bodies(path)
        sites = ledger.call_sites(path, parsed)
        counts['objects'] += 1
        for name in d.b.sizes(str(path)):
            counts['emitted_defining_bodies'] += 1
            if name not in sizes:
                counts['candidate_only_bodies'] += 1
                continue
            seen.add(name)
            counts['shared_defining_bodies'] += 1
            measured = compare(reference[name], parsed[name], reference_calls.get(name, []), sites[name])
            counts['unresolved_callee_sites'] += sum('target' not in s for s in sites[name])
            if not measured['sequence_equal']:
                counts['unequal_known_call_sequences'] += 1
                continue
            counts['equal_known_call_sequences'] += 1
            for pair in measured['pairs']:
                counts['aligned_known_calls'] += 1
                rejected = pair['original_use']['refusals'] + pair['retained_use']['refusals']
                if rejected:
                    counts['refused_call_pairs'] += 1
                    refusals.update(set(rejected))
                else:
                    counts['proved_extension_mode_pairs'] += 1
            for nomination in measured['nominations']:
                nominations.append({'symbol': name, 'object': object_name, 'source': source,
                                    'original_bytes': sizes[name],
                                    'legacy_scope': ledger.legacy_scope(source, sizes[name]),
                                    **nomination})
    if {path.name: sha(path) for path in args.objects.glob('*.o')} != hashes or \
            sha(args.reference) != snapshot['reference_sha256']:
        raise ValueError('screen inputs changed while running')
    output = {'revision': snapshot['revision'], 'baseline_manifest_sha256': sha(args.baseline_manifest),
              'reference_sha256': sha(args.reference), 'objects': hashes, 'build_config': config,
              'counts': dict(counts), 'unique_shared_names': len(seen),
              'reference_function_count': len(sizes), 'missing_reference_names': sorted(set(sizes)-seen),
              'refusal_reasons': dict(refusals), 'real_controls': real_controls,
              'nominations': nominations,
              'limits': ['Known-call sequence equality aligns ordinals, not CFG paths or argument values.',
                         'No production branch edge is granted without an independent ownership proof.',
                         'Only first AL/AX extensions are classified; other uses/aliases/calls refuse.',
                         'Missing and indirect targets remain unproved; no source/profile causes inferred.',
                         'Screen includes exact, large, fax and V34 functions and every defining copy.',
                         'Pinned object hashes establish artifact identity, not original compilation provenance.']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2)+'\n')
    print('return-mode screen:', dict(counts), '; unique shared', len(seen), '/', len(sizes),
          '; nominations', len(nominations))
    for row in nominations:
        print(row['symbol'], row['target'], row['original_bytes'], row['legacy_scope'],
              [v['kind'] for v in row['original_use']['outcomes']],
              [v['kind'] for v in row['retained_use']['outcomes']])


if __name__ == '__main__':
    main()
