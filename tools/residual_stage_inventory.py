#!/usr/bin/env python3
"""Inventory worst-copy REGALLOC/BYTES bodies without inferring compiler causes."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import subprocess
import playbook_small_patterns as d

RANK = {'EXACT': 0, 'UNRESOLVED': 1, 'REGALLOC': 2, 'RELOC': 3,
        'BYTES': 4, 'SIZE': 5, 'NODATA': 6}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--objects', type=Path, default=d.ROOT/'build/tc_out')
    ap.add_argument('--census', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    assert not d.b._staleness()[0], 'production cache stale: run make tc'
    census = json.loads(args.census.read_text())
    config = (args.objects/'.build-config').read_text()
    assert '-DDSPLIB_REPRODUCE_BUGS' in config
    exact = set(census['exact_symbols'])
    blob = d.b.sizes(d.b.BLOB)
    objects, hashes, sources = defaultdict(list), {}, {}
    for line in (args.objects/'tc_manifest.txt').read_text().splitlines():
        obj, source = line.split()
        path = args.objects/obj
        hashes[obj] = hashlib.sha256(path.read_bytes()).hexdigest()
        sources[obj] = source
        for symbol in d.b.sizes(str(path)):
            if symbol in blob:
                objects[symbol].append(obj)
    assert len(objects) == census['compared']
    counts = Counter(EXACT=len(exact))
    candidates = []
    for symbol in sorted(set(objects)-exact):
        original = d.b.body(d.b.BLOB, symbol)
        scored = []
        for obj in sorted(objects[symbol]):
            path = str(args.objects/obj)
            score = d.b.verdict(*original, *d.b.body(path, symbol))
            scored.append((score, obj))
        (raw_grade, delta), worst = max(scored, key=lambda row: RANK[row[0][0]])
        why = None
        grade = raw_grade
        if grade not in ('EXACT', 'UNRESOLVED', 'NODATA', 'RELOC'):
            why = d.b.alpha_why(d.b.insns(d.b.BLOB, symbol), d.b.insns(str(args.objects/worst), symbol))
            if why is None:
                grade = 'REGALLOC'
        counts[grade] += 1
        if grade not in ('REGALLOC', 'BYTES'):
            continue
        left = d.b.insns(d.b.BLOB, symbol)
        right = d.b.insns(str(args.objects/worst), symbol)
        candidates.append({'symbol': symbol, 'grade': grade, 'raw_grade': raw_grade,
                           'raw_delta': delta, 'blob_bytes': blob[symbol],
                           'source': sources[worst], 'worst_object': worst,
                           'definitions': [{'object': obj, 'source': sources[obj],
                                            'raw_verdict': list(score)} for score, obj in scored],
                           'alpha_first_rejection': why,
                           'original_instructions': left, 'retained_instructions': right,
                           'compiler_stage_cause': 'not established'})
    expected = {k.upper(): census[k] for k in ('exact', 'regalloc', 'bytes', 'size', 'unresolved', 'reloc', 'nodata')}
    assert all(counts[k] == v for k, v in expected.items()), (dict(counts), expected)
    candidates.sort(key=lambda row: (row['grade'], row['blob_bytes'], row['symbol']))
    names = [row['symbol'] for row in candidates]
    demangled = subprocess.check_output(['c++filt']+names, text=True).splitlines()
    assert len(demangled) == len(names)
    for row, name in zip(candidates, demangled):
        row['demangled'] = name
    # Real comparator positive/negative controls, outside count inference.
    assert next(row for row in candidates if row['symbol'] == '_iir_filter_create')['grade'] == 'REGALLOC'
    control = '_ZN8V90Modem14setSessionFlagEj'
    assert control in exact
    assert d.b.verdict(*d.b.body(d.b.BLOB, control), *d.b.body(str(args.objects/'src_pump_v90_V90Modem.cpp.o'), control))[0] == 'EXACT'
    output = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
              'build_config': config, 'census_sha256': hashlib.sha256(args.census.read_bytes()).hexdigest(),
              'object_sha256': hashes, 'unique_census': dict(counts), 'candidate_count': len(candidates),
              'source_TU_count': len({r['source'] for r in candidates}), 'candidates': candidates,
              'scope': 'worst defining copy scored as canonical census; stage cause deliberately unassigned'}
    args.output.write_text(json.dumps(output, indent=2)+'\n')
    print(dict(counts), len(candidates), 'candidates /', output['source_TU_count'], 'TUs')
    for row in candidates:
        print(row['grade'], row['blob_bytes'], row['demangled'], row['source'], row['alpha_first_rejection'])


if __name__ == '__main__':
    main()
