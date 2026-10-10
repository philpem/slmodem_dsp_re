#!/usr/bin/env python3
"""Nominate named direct call/sibling mismatches; do not infer source or types."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import playbook_small_patterns as d


def forms(path, symbol):
    raw, relocs = d.b.body(str(path), symbol)
    assert raw is not None
    calls, jumps = Counter(), Counter()
    for offset, (kind, target) in relocs.items():
        if kind != 'R_386_PC32' or target[0] != 'symbol' or offset < 1:
            continue
        if raw[offset-1] == 0xe8:
            calls[tuple(target)] += 1
        elif raw[offset-1] == 0xe9:
            jumps[tuple(target)] += 1
    return calls, jumps


def compare(original, retained):
    a, b = original
    c, e = retained
    if a+b != c+e:
        return []
    return [list(target) for target in sorted(a) if a[target] > c[target] and e[target] > b[target]]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, default=d.ROOT/'build/tc_out')
    parser.add_argument('--census', type=Path, default=d.ROOT/'build/byteident-v34-rate-lifetime.json')
    parser.add_argument('--positive-object', type=Path,
                        default=d.ROOT/'build/v34-rxrate-lifetime/VPcmV34Main/baseline/candidate.o')
    parser.add_argument('--max-bytes', type=int, default=650)
    parser.add_argument('--output', type=Path, default=d.ROOT/'build/small-call-result-screen.json')
    args = parser.parse_args()
    config = (args.objects/'.build-config').read_text()
    assert '-DDSPLIB_REPRODUCE_BUGS' in config
    exact = set(json.loads(args.census.read_text())['exact_symbols'])
    original_sizes = d.b.sizes(d.b.BLOB)
    symbol = 'VPcmV34GetCurrentRxBitRate'
    positive = compare(forms(d.b.BLOB, symbol), forms(args.positive_object, symbol))
    assert len(positive) == 1 and positive[0][1] == '_ZNK14V90Demodulator10getBitRateEv'
    current = args.objects/'src_pump_v34_VPcmV34Main.cpp.o'
    assert d.b.verdict(*d.b.body(d.b.BLOB, symbol), *d.b.body(str(current), symbol))[0] == 'EXACT'
    assert compare(forms(d.b.BLOB, symbol), forms(current, symbol)) == []
    counts = Counter()
    rows = []
    object_hashes = {}
    for line in (args.objects/'tc_manifest.txt').read_text().splitlines():
        object_name, source = line.split()
        path = args.objects/object_name
        object_hashes[object_name] = hashlib.sha256(path.read_bytes()).hexdigest()
        counts['TUs'] += 1
        for name, size in d.b.sizes(str(path)).items():
            if name not in original_sizes:
                continue
            counts['shared_emitted_bodies'] += 1
            if name in exact:
                counts['exact_excluded'] += 1
                continue
            if not 0 < original_sizes[name] <= args.max_bytes:
                continue
            counts['small_nonexact_bodies'] += 1
            left, right = forms(d.b.BLOB, name), forms(path, name)
            if left[0]+left[1] != right[0]+right[1]:
                counts['different_direct_transfer_multiset_excluded'] += 1
                continue
            targets = compare(left, right)
            if not targets:
                continue
            counts['nominated_emitted_bodies'] += 1
            rows.append({'symbol': name, 'source': source, 'object': object_name,
                         'blob_bytes': original_sizes[name], 'retained_bytes': size,
                         'call_to_sibling_targets': targets})
    rows.sort(key=lambda r: (r['blob_bytes'], r['source'], r['symbol']))
    args.output.write_text(json.dumps({'revision': subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=d.ROOT, text=True).strip(),
        'config': config, 'census_sha256': hashlib.sha256(args.census.read_bytes()).hexdigest(),
        'object_sha256': object_hashes,
        'counts': dict(counts), 'candidates': rows,
        'positive_control': positive, 'current_exact_negative_control': symbol,
        'limits': 'named PC32 direct transfers only; no indirect calls, source, alias or layout inference'}, indent=2)+'\n')
    print(dict(counts))
    for row in rows:
        print(row['blob_bytes'], row['retained_bytes'], row['source'], row['symbol'], row['call_to_sibling_targets'])


if __name__ == '__main__':
    main()
