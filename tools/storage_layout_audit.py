#!/usr/bin/env python3
"""Inventory allocated OBJECT/COMMON storage without equating gaps to variables.

COMMON st_value is an alignment requirement; positioned symbols expose only
their section's alignment, not their original declaration's alignment. Duplicate
local names remain multiset records. No source-owner inference is made from the
nearest FILE record for global symbols.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent / 'toolchain'))
import partialcmp as elf


def inventory(path):
    sections = elf.sections(str(path))
    symbols, _ = elf.symbols(str(path), sections)
    owners = {entry['name']: entry for entry in sections}
    records = []
    for kind, bind, visibility, section, value, size, name in symbols:
        if kind != 'OBJECT' or section not in owners and section != 'COM':
            continue
        records.append({'name': name, 'bind': bind, 'visibility': visibility,
                        'section': section, 'size': size,
                        'value': value, 'common_alignment': value if section == 'COM' else None})
    nobits = []
    for entry in sections:
        if entry['type'] != 'NOBITS':
            continue
        intervals = sorted((r['value'], r['value'] + r['size']) for r in records
                           if r['section'] == entry['name'] and r['size'])
        covered, end = 0, 0
        for lo, hi in intervals:
            assert 0 <= lo <= hi <= entry['size']
            covered += max(0, hi - max(lo, end))
            end = max(end, hi)
        nobits.append({'section': entry['name'], 'size': entry['size'],
                       'alignment': entry['align'], 'named_union_bytes': covered,
                       'uncovered_bytes': entry['size'] - covered})
    return {'path': str(path), 'sha256': hashlib.sha256(Path(path).read_bytes()).hexdigest(),
            'allocated_section_count': sum('A' in e['flags'] for e in sections),
            'symbol_count': len(symbols), 'sections': sections, 'storage': records,
            'nobits': nobits}


def compare(reference, candidate):
    def signature(row):
        return (row['name'], row['bind'], row['visibility'], row['section'],
                row['size'], row['common_alignment'])
    a = Counter(map(signature, reference['storage']))
    b = Counter(map(signature, candidate['storage']))
    def bss(rows):
        names = {e['section'] for e in rows['nobits']}
        return Counter(signature(r) for r in rows['storage'] if r['section'] in names)
    return {'reference_only_storage_records': list((a-b).elements()),
            'candidate_only_storage_records': list((b-a).elements()),
            'matched_storage_records': sum((a & b).values()),
            'nobits_named_multiset_equal': bss(reference) == bss(candidate),
            'nobits_extent_delta': sum(e['size'] for e in reference['nobits']) - sum(e['size'] for e in candidate['nobits']),
            'nobits_uncovered_delta': sum(e['uncovered_bytes'] for e in reference['nobits']) - sum(e['uncovered_bytes'] for e in candidate['nobits'])}


def controls():
    with tempfile.TemporaryDirectory(prefix='storage-audit-') as folder:
        root = Path(folder)
        def build(name, align=5, width=4, common_align=16):
            source = root / (name + '.s')
            target = root / (name + '.o')
            source.write_text('.bss\n.type first,@object\nfirst:\n.zero 4\n.size first,4\n'
                              '.p2align %d\n.type second,@object\nsecond:\n.zero %d\n.size second,%d\n'
                              '.comm common_slot,8,%d\n' % (align, width, width, common_align))
            subprocess.run(['as', '--32', '-o', str(target), str(source)], check=True)
            return inventory(target)
        baseline = build('baseline')
        same = compare(baseline, baseline)
        alignment = compare(baseline, build('alignment', align=4))
        width = compare(baseline, build('width', width=8))
        common = compare(baseline, build('common', common_align=8))
        assert not same['reference_only_storage_records'] and same['nobits_extent_delta'] == 0
        assert alignment['nobits_named_multiset_equal'] and alignment['nobits_extent_delta'] == 16
        assert alignment['nobits_uncovered_delta'] == 16
        assert not width['nobits_named_multiset_equal'] and width['reference_only_storage_records']
        assert common['reference_only_storage_records'] == [('common_slot', 'GLOBAL', 'DEFAULT', 'COM', 8, 16)]
    return 4


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('reference', type=Path)
    ap.add_argument('candidate', type=Path)
    ap.add_argument('--json', type=Path, required=True)
    args = ap.parse_args()
    denominator = controls()
    reference, candidate = inventory(args.reference), inventory(args.candidate)
    result = compare(reference, candidate)
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps({'reference': reference, 'candidate': candidate,
                                   'comparison': result, 'controls': denominator}, indent=2) + '\n')
    print('%d controls passed: equal, alignment-only gap, named-size mutation, COMMON-alignment mutation' % denominator)
    print('allocated storage records: reference %d, candidate %d, matched %d' %
          (len(reference['storage']), len(candidate['storage']), result['matched_storage_records']))
    print('NOBITS named multiset equal: %s; extent delta %d; uncovered delta %d' %
          (result['nobits_named_multiset_equal'], result['nobits_extent_delta'], result['nobits_uncovered_delta']))


if __name__ == '__main__':
    main()
