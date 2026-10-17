#!/usr/bin/env python3
"""Report defining-TU witnesses from unique LOCAL-object absolute relocations.

This is separate from name/bracket voting. GLOBAL data, anonymous gaps,
one-past addresses, overlapping objects and conflicting FILE owners do not
establish an owner. Symbol and FILE occurrences, not names, are identities.
The model assumes ordinary compiler/assembler inputs, without post-link
symbol localization or deliberately manufactured cross-input section offsets.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import elfinfo
from toolchain import partialcmp


def build(path):
    partialcmp.validate_elf(str(path))
    raw = Path(path).read_bytes()
    sections = {s['index']: s for s in partialcmp.sections(str(path), include_nonallocated=True)}
    symbols = elfinfo.read_symbols(str(path))
    by_num = {s.num: s for s in symbols}
    owners, current, counts = {}, None, Counter()
    for symbol in symbols:
        if symbol.type == 'FILE':
            current = None
            if elfinfo.is_source_file(symbol.name):
                current = {'file': symbol.name, 'file_symtab_index': symbol.num,
                           'file_occurrence': counts[symbol.name]}
                counts[symbol.name] += 1
        elif symbol.bind == 'LOCAL' and current:
            owners[symbol.num] = current
    objects = [s for s in symbols if s.type == 'OBJECT' and s.size > 0 and s.ndx.isdigit()
               and int(s.ndx) in sections and 'A' in sections[int(s.ndx)]['flags']
               and 'X' not in sections[int(s.ndx)]['flags']]
    functions = [s for s in symbols if s.type == 'FUNC' and s.size > 0 and s.ndx.isdigit()
                 and int(s.ndx) in sections and 'X' in sections[int(s.ndx)]['flags']]
    records = {s.num: {'function': dict(s._asdict()), 'witnesses': [], 'rejections': Counter()} for s in functions}
    relocation_count = 0
    for section in sections.values():
        if section['type'] != 'REL' or section['info'] not in sections:
            continue
        source = sections[section['info']]
        if 'X' not in source['flags']:
            continue
        assert section['entsize'] == 8 and section['size'] % 8 == 0
        for offset in range(section['offset'], section['offset'] + section['size'], 8):
            at, info = struct.unpack_from('<II', raw, offset)
            candidates = [f for f in functions if int(f.ndx) == source['index'] and f.value <= at < f.value + f.size]
            if not candidates:
                continue
            relocation_count += 1
            target = by_num[info >> 8]
            reason, match, address = None, None, None
            if 'G' in source['flags'] or source['name'].startswith('.gnu.linkonce.'):
                reason = 'COMDAT-or-linkonce-code'
            elif info & 255 != 1:
                reason = 'not-R_386_32'
            elif not target.ndx.isdigit() or int(target.ndx) not in sections:
                reason = 'undefined-absolute-or-COMMON-target'
            elif 'A' not in sections[int(target.ndx)]['flags'] or 'X' in sections[int(target.ndx)]['flags']:
                reason = 'not-allocated-nonexecuting-data'
            elif any(flag in sections[int(target.ndx)]['flags'] for flag in ('G','M')) or sections[int(target.ndx)]['name'].startswith('.gnu.linkonce.'):
                reason = 'mergeable-COMDAT-or-linkonce-data'
            elif target.type not in ('SECTION', 'OBJECT'):
                reason = 'not-section-or-object-target'
            elif target.type == 'OBJECT' and target.bind != 'LOCAL':
                reason = 'GLOBAL-or-WEAK-data-does-not-establish-owner'
            else:
                assert at + 4 <= source['size']
                addend = struct.unpack_from('<I', raw, source['offset'] + at)[0]
                address = (target.value + addend) & 0xffffffff
                matches = [o for o in objects if o.ndx == target.ndx and o.value <= address < o.value + o.size]
                if len(matches) != 1:
                    reason = 'uncovered-or-overlapping-object-address'
                else:
                    match = matches[0]
                    if target.type == 'OBJECT' and match.num != target.num:
                        reason = 'named-object-addend-outside-object'
                    elif match.bind != 'LOCAL' or match.num not in owners:
                        reason = 'object-without-LOCAL-FILE-owner'
            for function in candidates:
                record = records[function.num]
                if at + 4 > function.value + function.size:
                    record['rejections']['relocation-field-crosses-function-end'] += 1
                elif reason:
                    record['rejections'][reason] += 1
                else:
                    record['witnesses'].append({'relocation_section_index': section['index'],
                        'field_section_offset': at, 'function_offset': at - function.value,
                        'relocation_symbol_index': target.num, 'target_address': address,
                        'object': dict(match._asdict()), 'owner': owners[match.num],
                        'object_section': {'index': int(match.ndx),
                                           'name': sections[int(match.ndx)]['name'],
                                           'range': [match.value, match.value + match.size]},
                        'object_offset': address - match.value})
    for number, record in records.items():
        unique = {w['owner']['file_symtab_index'] for w in record['witnesses']}
        record['status'] = 'unique-LOCAL-data-owner' if len(unique) == 1 else 'conflicting-LOCAL-data-owners' if unique else 'no-witness'
        record['owner'] = record['witnesses'][0]['owner'] if len(unique) == 1 else None
        record['local_function_FILE_truth'] = owners.get(number) if record['function']['bind'] == 'LOCAL' else None
    global_records = [r for r in records.values() if r['function']['bind'] in ('GLOBAL', 'WEAK')]
    return {'object': str(path), 'sha256': hashlib.sha256(raw).hexdigest(),
            'sized_function_occurrences': len(records), 'GLOBAL_WEAK_function_occurrences': len(global_records),
            'code_relocations_inside_functions': relocation_count,
            'global_status_counts': dict(Counter(r['status'] for r in global_records)),
            'records': records,
            'limits': ['No GLOBAL data defining-TU inference, including SMCv32_CFG.',
                       'Mergeable and COMDAT/linkonce sections are excluded; deduplication may cross input ownership.',
                       'No source-path equivalence or automatic attribution override.',
                       'Assumes ordinary inputs without post-link localization/cross-input section arithmetic.']}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('object', nargs='?', type=Path)
    ap.add_argument('--json', type=Path)
    ap.add_argument('--self-test', action='store_true')
    ap.add_argument('--manifest', type=Path,
                    help='optional current object/source manifest for ranked source-TU nominations, not automatic assignments')
    args = ap.parse_args()
    if args.self_test:
        from local_data_ownership_selftest import run
        run(args.object)
        return
    if not args.object or not args.json:
        ap.error('object and --json are required unless --self-test')
    report = build(args.object)
    if args.manifest:
        report['source_screen'] = source_screen(report, args.manifest)
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, indent=2) + '\n')
    print('%d sized functions; %d GLOBAL/WEAK; %d code relocations inside functions' %
          (report['sized_function_occurrences'], report['GLOBAL_WEAK_function_occurrences'], report['code_relocations_inside_functions']))
    print('GLOBAL/WEAK witness statuses:', report['global_status_counts'])
    if args.manifest:
        screen = report['source_screen']
        print('%d manifest objects / %d original witnessed functions with live definitions; %d differing source basenames (nominations only)' %
              (screen['manifest_objects'], screen['witnessed_functions_with_definitions'], len(screen['nominations'])))
        for row in screen['nominations'][:15]:
            print('  gap=%+d %s: %s -> original LOCAL owner %s' %
                  (row['size_delta'], row['function'], row['source'], row['owner']['file']))


def source_screen(report, manifest):
    definitions, objects = {}, []
    for line in manifest.read_text().splitlines():
        if not line.strip():
            continue
        obj, source = line.split()
        path = manifest.parent / obj
        objects.append({'object': obj, 'source': source,
                        'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        for symbol in elfinfo.read_symbols(str(path)):
            if symbol.type == 'FUNC' and symbol.bind in ('GLOBAL', 'WEAK') and symbol.ndx.isdigit():
                definitions.setdefault(symbol.name, []).append((source, obj, symbol))
    nominations, witnessed = [], 0
    for record in report['records'].values():
        if record['status'] != 'unique-LOCAL-data-owner' or record['function']['bind'] not in ('GLOBAL','WEAK'):
            continue
        found = definitions.get(record['function']['name'], [])
        witnessed += bool(found)
        for source, obj, symbol in found:
            if Path(source).name == record['owner']['file']:
                continue
            nominations.append({'function': record['function']['name'],
                                'reference_function_index': record['function']['num'],
                                'source': source, 'object': obj, 'owner': record['owner'],
                                'size_delta': symbol.size - record['function']['size'],
                                'original_bytes': record['function']['size'],
                                'witnesses': record['witnesses'],
                                'status': 'nomination: reconstructed filename aliases may differ; review source/helper boundaries'})
    nominations.sort(key=lambda r: (abs(r['size_delta']), r['original_bytes'], r['function'], r['source']))
    return {'manifest': str(manifest), 'manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
            'manifest_objects': len(objects), 'objects': objects,
            'witnessed_functions_with_definitions': witnessed, 'nominations': nominations,
            'limits': 'Recorded build only; manifest/source freshness not inferred from timestamps. No filename-alias or source correction proof.'}


if __name__ == '__main__':
    main()
