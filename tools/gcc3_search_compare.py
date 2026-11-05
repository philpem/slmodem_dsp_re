#!/usr/bin/env python3
"""Compare a full-TU variant: function grades and emitted layout are separate.

Read-only object analysis; compiler/source provenance and differential validation
remain the caller's responsibility. Raw data differences require relocation-aware
review and are not automatically semantic changes.
"""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import subprocess
import sys

TOOLS = Path(__file__).resolve().parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
import dis  # keep tools/dis.py from shadowing stdlib
from elftools.elf.elffile import ELFFile
from elftools.common.exceptions import ELFError
sys.path.insert(0, str(TOOLS))
import gcc3_search_pilot as pilot


def grade(api, blob, obj, symbol):
    raw = api.verdict(*api.body(blob, symbol), *api.body(obj, symbol))
    final = raw
    # Same grade-1 boundary as byteident.main; never override target failures.
    if raw[0] not in ('EXACT', 'UNRESOLVED', 'NODATA', 'RELOC'):
        if api.alpha_equal(api.insns(blob, symbol), api.insns(obj, symbol)):
            final = ('REGALLOC', 0)
    return {'raw': list(raw), 'grade': list(final)}


def layout(path, names):
    """Address groups within each section; no invented cross-section order."""
    groups = defaultdict(lambda: defaultdict(list))
    seen = set()
    with open(path, 'rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        if table is None:
            raise ValueError('missing symbol table: ' + path)
        for sym in table.iter_symbols():
            if sym.name not in names or sym['st_info']['type'] != 'STT_FUNC':
                continue
            idx = sym['st_shndx']
            if not isinstance(idx, int) or not sym['st_size']:
                continue
            if sym.name in seen:
                raise ValueError('ambiguous function layout: ' + sym.name)
            seen.add(sym.name)
            groups[elf.get_section(idx).name][sym['st_value']].append(sym.name)
    if seen != set(names):
        raise ValueError('layout missing sized functions: ' + ', '.join(sorted(set(names) - seen)))
    return {section: [sorted(row) for _, row in sorted(addresses.items())]
            for section, addresses in sorted(groups.items())}


def identity(path):
    return {'path': path, 'sha256': hashlib.sha256(Path(path).read_bytes()).hexdigest()}


def compare(baseline, candidate, blob=None):
    sys.path.insert(0, str(TOOLS / 'toolchain'))
    api = pilot.load_module('gcc3_compare_byteident', TOOLS / 'toolchain/byteident.py')
    baseline, candidate = str(Path(baseline).resolve()), str(Path(candidate).resolve())
    blob = str(Path(blob or api.BLOB).resolve())
    for path in (blob, baseline, candidate):
        if not Path(path).is_file():
            raise ValueError('missing object: ' + path)
    inventory = {key: api.sizes(path) for key, path in
                 [('reference', blob), ('baseline', baseline), ('candidate', candidate)]}
    names = sorted(set(inventory['reference']) & set(inventory['baseline']) & set(inventory['candidate']))
    if not names:
        raise ValueError('shared sized-function denominator is 0')
    rows = {name: {'baseline': grade(api, blob, baseline, name),
                   'candidate': grade(api, blob, candidate, name)} for name in names}
    transitions = defaultdict(list)
    for name, row in rows.items():
        transitions[row['baseline']['grade'][0] + ' -> ' + row['candidate']['grade'][0]].append(name)
    orders = {key: layout(path, names) for key, path in
              [('reference', blob), ('baseline', baseline), ('candidate', candidate)]}
    baseline_names, candidate_names = set(inventory['baseline']), set(inventory['candidate'])
    common = sorted(baseline_names & candidate_names)
    def members(side, kind):
        return {n for n, row in rows.items() if row[side]['grade'][0] == kind}
    changes = {}
    for kind in ('EXACT', 'REGALLOC'):
        before, after = members('baseline', kind), members('candidate', kind)
        changes[kind] = {'gained': sorted(after - before), 'lost': sorted(before - after)}
    return {'schema': 'gcc3-variant-comparison-v1', 'status': 'valid',
            'analysis_sha256': {p.name: identity(str(p))['sha256'] for p in
                                (Path(__file__), TOOLS / 'gcc3_search_pilot.py',
                                 TOOLS / 'toolchain/byteident.py')},
            'inputs': {key: identity(path) for key, path in
                       [('reference', blob), ('baseline', baseline), ('candidate', candidate)]},
            'shared_sized_functions': len(names), 'functions': rows,
            'grade_transitions': dict(sorted(transitions.items())), 'grade_changes': changes,
            'inventory': {'baseline_count': len(baseline_names), 'candidate_count': len(candidate_names),
                          'missing': sorted(baseline_names - candidate_names),
                          'added': sorted(candidate_names - baseline_names),
                          'baseline_without_reference': sorted(baseline_names - set(inventory['reference'])),
                          'candidate_without_reference': sorted(candidate_names - set(inventory['reference']))},
            'changed_bodies_or_relocations': [n for n in common if api.body(baseline, n) != api.body(candidate, n)],
            'layout': {'shared_function_denominator': len(names), 'section_address_groups': orders,
                       'baseline_matches_reference': orders['baseline'] == orders['reference'],
                       'candidate_matches_reference': orders['candidate'] == orders['reference'],
                       'candidate_matches_baseline': orders['candidate'] == orders['baseline']},
            'exports_unchanged': pilot.exported_bindings(baseline) == pilot.exported_bindings(candidate),
            'allocated_data_raw_unchanged': pilot.allocated_data(baseline) == pilot.allocated_data(candidate),
            'scope': 'Grades cover only functions shared by all three objects. Missing/added functions '
                     'are reported separately. Layout compares relative address groups within sections, '
                     'not absolute placement or source definition order. No adoption decision or '
                     'compiler/differential provenance is inferred.'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True)
    parser.add_argument('--candidate', required=True)
    parser.add_argument('--blob')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    stream = None
    try:
        stream = args.output.open('x')
        report = compare(args.baseline, args.candidate, args.blob)
        json.dump(report, stream, indent=2, sort_keys=True)
        stream.write('\n')
        print('valid: %d shared functions; EXACT +%d/-%d, REGALLOC +%d/-%d; emitted order matches reference: %s' %
              (report['shared_sized_functions'], len(report['grade_changes']['EXACT']['gained']),
               len(report['grade_changes']['EXACT']['lost']), len(report['grade_changes']['REGALLOC']['gained']),
               len(report['grade_changes']['REGALLOC']['lost']), report['layout']['candidate_matches_reference']))
        return 0
    except (OSError, ValueError, RuntimeError, ELFError, subprocess.SubprocessError) as exc:
        if stream is not None:
            stream.seek(0)
            stream.truncate()
            json.dump({'status': 'invalid', 'shared_sized_functions': None, 'error': str(exc)}, stream)
            stream.write('\n')
        print('invalid: denominator not established: ' + str(exc), file=sys.stderr)
        return 1
    finally:
        if stream is not None:
            stream.close()


if __name__ == '__main__':
    raise SystemExit(main())
