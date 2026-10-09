#!/usr/bin/env python3
"""Object-entry V90 C++ shortlist; grades duplicate templates per TU."""
import argparse
import json
from pathlib import Path
import playbook_small_patterns as d


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--max-bytes', type=int, default=400)
    args = parser.parse_args()
    original = d.b.sizes(d.b.BLOB)
    rows, eligible, excluded = [], [], []
    mapped = small = exact = 0
    for obj in sorted(args.objects.glob('src_pump_v90_*.cpp.o')):
        if 'PreFilter' in obj.name:
            excluded.append(obj.name)
            continue
        eligible.append(obj.name)
        for name, size in d.b.sizes(str(obj)).items():
            if name not in original:
                continue
            mapped += 1
            if original[name] > args.max_bytes:
                continue
            small += 1
            grade = d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))
            if grade[0] == 'EXACT':
                exact += 1
                continue
            rows.append({'object': obj.name, 'symbol': name, 'original_size': original[name],
                         'retained_size': size, 'grade': grade})
    rows.sort(key=lambda row: (row['original_size'], row['object'], row['symbol']))
    result = {'eligible_translation_units': len(eligible), 'eligible_objects': eligible,
              'excluded_prefilter_objects': excluded,
              'original_mapped_emitted_entries_all_sizes': mapped,
              'max_original_bytes': args.max_bytes, 'small_mapped_entries': small,
              'small_exact_entries': exact, 'small_nonexact_entries': len(rows),
              'duplicate_template_instances_graded_per_object': True, 'rows': rows}
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print('%d TUs / %d mapped entries; <=%d bytes %d exact + %d nonexact = %d' % (
        len(eligible), mapped, args.max_bytes, exact, len(rows), small))


if __name__ == '__main__':
    main()
