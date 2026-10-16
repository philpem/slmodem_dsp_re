#!/usr/bin/env python3
"""Archive resolved local-call boundaries; identity is not a prototype witness.

The input ledger must carry unique exact ELF ownership for each local target.
Only equal complete local-call sequences are paired. All sites, including refused
sequences, remain in the output. Windows are a manual review aid, not def/use.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess

from narrow_call_boundary_screen import bodies, extensions, sha

ROOT = Path(__file__).resolve().parents[1]


def local_sites(calls):
    return [c for c in calls if c['kind'] == 'local-direct']


def window(rows, call, radius):
    positions = [i for i, r in enumerate(rows)
                 if r['address'] == call['address'] and r['mnemonic'] == 'call']
    assert len(positions) == 1, call
    i = positions[0]
    return {'before': rows[max(0, i-radius):i],
            'call': rows[i], 'after': rows[i+1:i+1+radius]}


def paired(left, right):
    return [c['target'] for c in left] == [c['target'] for c in right]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ledger', type=Path, required=True)
    ap.add_argument('--objects', type=Path, required=True)
    ap.add_argument('--reference', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--window', type=int, default=16)
    args = ap.parse_args()
    assert args.window > 0
    ledger = json.loads(args.ledger.read_text())
    assert sha(args.reference) == ledger['reference_sha256']
    reference = bodies(args.reference)
    decoded = {}
    counts = Counter()
    report = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'],
              cwd=ROOT, text=True).strip(), 'ledger_revision': ledger['revision'],
              'ledger_sha256': sha(args.ledger), 'reference_sha256': sha(args.reference),
              'window': args.window, 'objects': {}, 'callers': []}
    for fn in ledger['functions']:
        for definition in fn['definitions']:
            right = local_sites(definition['calls'])
            if not right:
                continue
            name = definition['object']
            path = args.objects/name
            if name not in decoded:
                digest = sha(path)
                assert digest == ledger['object_sha256'][name], ('object drift', name)
                report['objects'][name] = digest
                decoded[name] = bodies(path)
            left = local_sites(fn['original_calls'])
            same = paired(left, right)
            counts['retained_local_sites'] += len(right)
            counts['original_sites_in_these_callers'] += len(left)
            counts['equal_sequence_callers' if same else 'refused_sequence_callers'] += 1
            counts['paired_sites' if same else 'refused_retained_sites'] += len(right)
            row = {'symbol': fn['symbol'], 'source': definition['source'],
                   'object': name, 'grade': fn['grade'],
                   'legacy_scope': definition['legacy_value_timing_scope'],
                   'equal_local_sequence': same,
                   'original': [{'target': c['target'], **window(reference[fn['symbol']], c, args.window)}
                                for c in left],
                   'retained': [{'target': c['target'], **window(decoded[name][fn['symbol']], c, args.window)}
                                for c in right]}
            if same:
                row['extra_extension_ordinals'] = [i for i, (a, b) in enumerate(
                    zip(row['original'], row['retained']))
                    if len(extensions(b['before'])) > len(extensions(a['before']))]
                counts['extra_extension_sites'] += len(row['extra_extension_ordinals'])
            report['callers'].append(row)
    # Positive sequence match, explicit target mismatch/reordering refusal,
    # real call-location checks above, and an extension detector positive/negative.
    a = [{'target': 'helper'}, {'target': 'other'}]
    assert paired(a, list(a)) and not paired(a, list(reversed(a)))
    assert not paired(a, [{'target': 'helper'}])
    ext = {'mnemonic': 'movswl', 'operands': '%ax,%eax'}
    assert extensions([ext]) == [ext] and extensions([]) == []
    report['controls'] = {'equal_sequence': True, 'reordered_refused': True,
                          'missing_call_refused': True, 'extension_positive': True,
                          'extension_negative': True}
    counts['objects'] = len(decoded)
    counts['callers'] = len(report['callers'])
    report['counts'] = dict(counts)
    report['limits'] = ['Manual windows do not prove argument width, return type, or value age.',
                        'Unequal local-call sequences are preserved but never zipped.',
                        'Indirect calls remain unresolved; no inferred target is introduced.']
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'counts': report['counts'], 'controls': report['controls']}, indent=2))


if __name__ == '__main__':
    main()
