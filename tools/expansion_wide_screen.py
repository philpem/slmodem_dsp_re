#!/usr/bin/env python3
"""Nominate original repeated-call graphs in medium C++ and DSP functions."""
import playbook_small_patterns as d
import argparse
import ast
from collections import Counter
import hashlib
import json
from pathlib import Path

CONTROL = '_ZN21V90ConstellationPower21calcModulusParametersEP16V90MappingParams'


def calls(path, name):
    targets = Counter()
    excluded = Counter()
    for mnemonic, operands in d.b.insns(str(path), name):
        if mnemonic != 'call':
            continue
        marker = '@R_386_PC32:'
        if marker not in operands:
            excluded['indirect_or_unrelocated'] += 1
            continue
        identity = ast.literal_eval(operands.split(marker, 1)[1])
        if identity[0] != 'symbol' or identity[2] != 0xfffffffc:
            excluded['non_symbol_or_interior_target'] += 1
            continue
        targets[identity[1]] += 1
    return targets, excluded


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, default=d.ROOT/'build/production-before')
    parser.add_argument('--baseline-json', type=Path, default=d.ROOT/'build/baseline-byteident.json')
    parser.add_argument('--positive-object', type=Path, default=d.ROOT/'build/expansion-wide-controls/rolled-constellation.o')
    parser.add_argument('--maximum-size', type=int, default=4096)
    args = parser.parse_args()
    baseline = json.loads(args.baseline_json.read_text())
    exact = set(baseline['exact_symbols'])
    reference = d.b.sizes(d.b.BLOB)
    report = {'maximum_original_bytes': args.maximum_size, 'baseline_exact': baseline['exact'],
              'translation_units': 0, 'emitted_bodies': 0, 'eligible_nonexact_bodies': 0,
              'bands': Counter(), 'excluded_calls': Counter(), 'nominations': [], 'rows': []}
    for source in sorted((d.ROOT/'src').rglob('*')):
        if not source.is_file() or source.suffix not in ('.cpp', '.c'):
            continue
        relative = str(source.relative_to(d.ROOT))
        if relative.startswith('src/pump/v34/'):
            continue
        if source.suffix != '.cpp' and not relative.startswith('src/dsp/'):
            continue
        obj = args.objects/(relative.replace('/', '_')+'.o')
        if not obj.exists():
            raise AssertionError(('missing scope object', relative))
        names = d.b.sizes(str(obj))
        report['translation_units'] += 1
        report['emitted_bodies'] += len(names)
        for name, size in names.items():
            if name in exact or name not in reference or reference[name] > args.maximum_size:
                continue
            blob_calls, blob_excluded = calls(d.b.BLOB, name)
            own_calls, own_excluded = calls(obj, name)
            report['excluded_calls'].update(blob_excluded)
            report['excluded_calls'].update(own_excluded)
            report['eligible_nonexact_bodies'] += 1
            report['bands']['1..800' if reference[name] <= 800 else '801..4096'] += 1
            repeats = {target: [count, own_calls[target]] for target, count in blob_calls.items()
                       if count >= 2 and own_calls[target] <= 1}
            row = {'source': relative, 'function': name, 'blob_size': reference[name],
                   'ours_size': size, 'repeat_differences': repeats,
                   'blob_calls': dict(blob_calls), 'ours_calls': dict(own_calls),
                   'verdict': list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name)))}
            report['rows'].append(row)
            if repeats:
                report['nominations'].append(row)
    positive_hash = hashlib.sha256(args.positive_object.read_bytes()).hexdigest()
    assert positive_hash == '3404b2abdb6a8bc21faf65c0249d55788441ddcf96c16d2a30dd086951a27a59', 'unknown positive fixture'
    blob, _ = calls(d.b.BLOB, CONTROL)
    rolled, _ = calls(args.positive_object, CONTROL)
    assert blob['__moddi3'] == 5 and blob['__divdi3'] == 6
    assert rolled['__moddi3'] == 1 and rolled['__divdi3'] == 2
    report['known_positive'] = {'function': CONTROL, 'original': dict(blob),
                                'rolled': dict(rolled), 'fired': True,
                                'object_sha256': hashlib.sha256(args.positive_object.read_bytes()).hexdigest()}
    (d.ROOT/'build/expansion-wide-screen.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({key: value for key, value in report.items() if key != 'rows'}, indent=2))


if __name__ == '__main__':
    main()
