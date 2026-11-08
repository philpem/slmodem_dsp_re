#!/usr/bin/env python3
"""Show conservative duplicate-function grades beside a supplied partial link.

Diagnostic only: never writes byteident's ratchet or changes its EXACT set.
The linked file is an observed artifact, not proof of the original link order.
Input hashes bind this report to its files; they do not prove how it was built.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import sys

from elftools.elf.elffile import ELFFile
import byteident

RANK = {name: i for i, name in enumerate(
    ('EXACT', 'UNRESOLVED', 'REGALLOC', 'RELOC', 'BYTES', 'SIZE', 'NODATA'))}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def functions(path):
    """Keep symbol records and section identity, including local collisions."""
    with open(path, 'rb') as stream:
        elf = ELFFile(stream)
        if (elf.elfclass != 32 or not elf.little_endian or
                elf['e_type'] != 'ET_REL' or elf['e_machine'] != 'EM_386'):
            raise ValueError('%s must be ELF32 little-endian i386 ET_REL' % path)
        table = elf.get_section_by_name('.symtab')
        if table is None:
            raise ValueError('%s has no symbol table' % path)
        rows = []
        for symbol in table.iter_symbols():
            if (symbol['st_info']['type'] != 'STT_FUNC' or
                    not isinstance(symbol['st_shndx'], int)):
                continue
            section = elf.get_section(symbol['st_shndx'])
            rows.append({'name': symbol.name,
                         'binding': symbol['st_info']['bind'],
                         'visibility': symbol['st_other']['visibility'],
                         'section': section.name, 'section_index': symbol['st_shndx'],
                         'value': symbol['st_value'], 'size': symbol['st_size'],
                         'linkonce_or_group': section.name.startswith('.gnu.linkonce.') or
                         bool(section['sh_flags'] & 0x200)})
        return rows


def inputs(manifest, directory):
    rows = []
    for number, line in enumerate(Path(manifest).read_text().splitlines(), 1):
        fields = line.split()
        if len(fields) != 2:
            raise ValueError('%s:%d must contain object and source' % (manifest, number))
        obj, source = fields
        if Path(obj).name != obj:
            raise ValueError('object names must be basenames: ' + obj)
        path = directory / obj
        if not path.is_file():
            raise ValueError('missing object: ' + str(path))
        rows.append((path, source))
    if not rows or len({p.name for p, _ in rows}) != len(rows):
        raise ValueError('empty manifest or repeated object')
    if len({s for _, s in rows}) != len(rows):
        raise ValueError('repeated source in manifest')
    extra = {p.name for p in directory.glob('*.o')} - {p.name for p, _ in rows}
    if extra:
        raise ValueError('objects outside manifest: ' + ', '.join(sorted(extra)))
    return rows


def grade(blob, candidate, name):
    a, ar = byteident.body(str(blob), name)
    b, br = byteident.body(str(candidate), name)
    raw, delta = byteident.verdict(a, ar, b, br)
    final = raw
    if raw not in ('EXACT', 'UNRESOLVED', 'NODATA', 'RELOC'):
        if byteident.alpha_equal(byteident.insns(str(blob), name),
                                 byteident.insns(str(candidate), name)):
            final = 'REGALLOC'
    return {'raw': raw, 'grade': final, 'delta': delta}


def exact_body_match(left, right):
    return (left[0] is not None and right[0] is not None and
            byteident.verdict(*left, *right)[0] == 'EXACT')


def audit(blob, manifest, linked, directory=None):
    directory = Path(directory) if directory else Path(manifest).parent
    rows = inputs(manifest, directory)
    reference = defaultdict(list)
    for row in functions(blob):
        reference[row['name']].append(row)
    linked_symbols = defaultdict(list)
    for row in functions(linked):
        linked_symbols[row['name']].append(row)
    definitions = defaultdict(list)
    object_hashes = {}
    for position, (path, source) in enumerate(rows):
        object_hashes[path.name] = digest(path)
        for row in functions(path):
            definitions[row['name']].append(dict(row, object=path.name,
                                                source=source, input_index=position))
    duplicates = {n: copies for n, copies in definitions.items() if len(copies) > 1}
    if not duplicates:
        raise ValueError('no repeated function names: diagnostic denominator is zero')
    output = []
    for name, copies in sorted(duplicates.items()):
        row = {'symbol': name, 'copies': [],
               'scope': ('local-name-collision' if any(c['binding'] == 'STB_LOCAL'
                                                      for c in copies)
                         else 'external-duplicate'),
               'reference_records': len(reference[name]),
               'linked_records': len(linked_symbols[name]),
               'linked_definition_records': linked_symbols[name]}
        comparable = len(reference[name]) == 1
        for copy in copies:
            entry = dict(copy)
            if comparable:
                entry.update(grade(blob, directory / copy['object'], name))
            row['copies'].append(entry)
        if comparable:
            # Exactly byteident's policy: worst RAW verdict, sorted filename
            # on ties, then grade-1 check for that chosen copy. Report
            # the full tie set independently of the declared link order.
            worst_rank = max(RANK[c['raw']] for c in row['copies'])
            worst = [c for c in row['copies'] if RANK[c['raw']] == worst_rank]
            # byteident uses sorted filenames, not link order, for tie selection.
            chosen = min(worst, key=lambda c: c['object'])
            row['conservative_grade'] = chosen['grade']
            row['conservative_object'] = chosen['object']
            row['worst_raw_tie_objects'] = [c['object'] for c in worst]
            row['linked'] = grade(blob, linked, name)
        matches = []
        if len(linked_symbols[name]) == 1:
            selected = byteident.body(str(linked), name)
            if selected[0] is not None:
                for copy in copies:
                    candidate = byteident.body(str(directory / copy['object']), name)
                    if exact_body_match(candidate, selected):
                        matches.append(copy['object'])
        row['linked_body_matching_inputs'] = matches
        row['input_supplier_proven'] = False
        output.append(row)
    summary = {'input_objects': len(rows),
               'input_function_records': sum(map(len, definitions.values())),
               'duplicate_names': len(output),
               'duplicate_definition_records': sum(len(r['copies']) for r in output),
               'external_duplicate_names': sum(r['scope'] == 'external-duplicate' for r in output),
               'local_collision_names': sum(r['scope'] == 'local-name-collision' for r in output),
               'reference_comparable_duplicate_names': sum('linked' in r for r in output),
               'linked_body_grades': dict(Counter(r['linked']['grade'] for r in output if 'linked' in r)),
               'conservative_grades': dict(Counter(r['conservative_grade'] for r in output if 'linked' in r)),
               'no_matching_input_body': sum(not r['linked_body_matching_inputs'] for r in output),
               'views_disagree': sum(r['conservative_grade'] != r['linked']['grade']
                                     for r in output if 'linked' in r)}
    return {'schema': 1, 'diagnostic_only': True,
            'selection_provenance': 'supplied linked artifact; manifest is declared input order, not proof of original order or replay',
            'identity_scope': 'byteident function bodies only; not whole-object identity, binding equivalence or semantic proof',
            'blob': {'path': str(blob), 'sha256': digest(blob)},
            'linked': {'path': str(linked), 'sha256': digest(linked)},
            'manifest': {'path': str(manifest), 'sha256': digest(manifest)},
            'object_sha256': object_hashes, 'summary': summary, 'duplicates': output}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blob', type=Path, default=Path(byteident.BLOB))
    parser.add_argument('--manifest', required=True, type=Path)
    parser.add_argument('--object-dir', type=Path)
    parser.add_argument('--linked', required=True, type=Path)
    parser.add_argument('--json-out', required=True, type=Path)
    args = parser.parse_args()
    report = audit(args.blob, args.manifest, args.linked, args.object_dir)
    args.json_out.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print('Duplicate-function diagnostic (ratchet unchanged)')
    for name, count in report['summary'].items():
        print('  %s: %s' % (name, count))
    for row in report['duplicates']:
        if 'linked' in row and (row['conservative_grade'] != 'EXACT' or
                                row['linked']['grade'] != 'EXACT'):
            print('  %s: conservative %s; supplied link %s; matching input copies %d' %
                  (row['symbol'], row['conservative_grade'], row['linked']['grade'],
                   len(row['linked_body_matching_inputs'])))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        sys.exit('linkedcopies: ' + str(error))
