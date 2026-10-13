#!/usr/bin/env python3
"""Nominate extra register extensions near matched calls; never infer a prototype.

Windows are deliberately only a screen: scheduling, unrelated arithmetic and
different control flow can all explain a nomination. Preserve both windows for
manual def/use review. Match only complete equal named-call sequences.
"""
import argparse
import playbook_small_patterns  # Preload stdlib dis before tools/dis.py can shadow it.
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
HEADER = re.compile(r'^([0-9a-f]+) <([^>]+)>:$')
INSN = re.compile(r'^\s*([0-9a-f]+):\t(\S+)\s*(.*)$')
RELOC = re.compile(r'^\s*[0-9a-f]+:\s+R_386_PC32\s+(\S+)\s*$')


def bodies(path):
    output = subprocess.check_output(['objdump', '-dr', '--no-show-raw-insn', str(path)], text=True)
    result = {}
    rows = None
    for line in output.splitlines():
        header = HEADER.match(line)
        if header:
            rows = result.setdefault(header[2], [])
            continue
        if rows is None:
            continue
        relocation = RELOC.match(line)
        if relocation:
            if rows and rows[-1]['mnemonic'] == 'call':
                rows[-1]['target'] = relocation[1]
            continue
        instruction = INSN.match(line)
        if instruction:
            rows.append({'address': instruction[1], 'mnemonic': instruction[2],
                         'operands': instruction[3]})
    return result


def calls(rows, window):
    return [(r['target'], rows[max(0, i-window):i]) for i, r in enumerate(rows)
            if r['mnemonic'] == 'call' and 'target' in r]


def extensions(rows):
    return [r for r in rows if (r['mnemonic'] in ('movswl', 'movzwl', 'movsbl', 'movzbl')
                               and re.fullmatch(r'%\w+,%\w+', r['operands']))
            or r['mnemonic'] == 'cwtl']


def compare(original, retained, window):
    left, right = calls(original, window), calls(retained, window)
    if [r[0] for r in left] != [r[0] for r in right]:
        return None
    return [{'ordinal': i, 'target': a[0], 'original_extensions': extensions(a[1]),
             'retained_extensions': extensions(b[1]), 'original_window': a[1], 'retained_window': b[1]}
            for i, (a, b) in enumerate(zip(left, right))
            if len(extensions(b[1])) > len(extensions(a[1]))]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, default=ROOT/'build/tc_out')
    parser.add_argument('--reference', type=Path,
                        default=Path('/mnt/zfs/dev/slmodem/claude_re/ref/slmodemd/dsplibs.o'))
    parser.add_argument('--window', type=int, default=8)
    parser.add_argument('--positive-object', type=Path,
                        default=ROOT/'build/gcc3-agc-rms-boundary/fpm_agc/length/candidate.o')
    parser.add_argument('--output', type=Path, default=ROOT/'build/narrow-call-boundary-screen.json')
    args = parser.parse_args()
    assert args.window > 0
    reference = bodies(args.reference)
    counts = Counter()
    report = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'reference_sha256': sha(args.reference), 'window': args.window,
              'config': (args.objects/'.build-config').read_text(), 'objects': {}, 'nominations': []}
    for path in sorted(args.objects.glob('*.o')):
        counts['objects'] += 1
        report['objects'][path.name] = sha(path)
        for name, rows in bodies(path).items():
            counts['emitted_bodies'] += 1
            if name not in reference:
                counts['without_reference'] += 1
                continue
            counts['shared_bodies'] += 1
            found = compare(reference[name], rows, args.window)
            if found is None:
                counts['different_named_call_sequence'] += 1
                continue
            counts['equal_named_call_sequence'] += 1
            counts['matched_calls'] += len(calls(rows, args.window))
            if found:
                counts['nominated_bodies'] += 1
                counts['nominated_calls'] += len(found)
                report['nominations'].append({'object': path.name, 'symbol': name, 'calls': found})
    # A historical real caller is the positive; identity is the negative.
    historical = args.positive_object
    positive = compare(reference['FPM_AGC_agc'], bodies(historical)['FPM_AGC_agc'], args.window)
    assert positive is not None and any(r['target'] == 'FPM_rms' for r in positive)
    assert compare(reference['FPM_AGC_agc'], reference['FPM_AGC_agc'], args.window) == []
    report['controls'] = {'historical_RMS_extra_extension': True, 'identity_negative': True,
                          'historical_object_sha256': sha(historical)}
    report['counts'] = dict(counts)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'counts': dict(counts), 'controls': report['controls']}, indent=2))
    for row in report['nominations']:
        print(row['object'], row['symbol'], [(r['target'], len(r['original_extensions']),
                                             len(r['retained_extensions'])) for r in row['calls']])


if __name__ == '__main__':
    main()
