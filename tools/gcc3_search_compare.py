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
import threading
import weakref

TOOLS = Path(__file__).resolve().parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
import dis  # keep tools/dis.py from shadowing stdlib
from elftools.elf.elffile import ELFFile
from elftools.common.exceptions import ELFError
sys.path.insert(0, str(TOOLS))
import gcc3_search_data as data_audit
import gcc3_search_pilot as pilot

_GLOBAL_COMPARISON_LOCK = threading.RLock()
_GLOBAL_PATH_DIGESTS = {}
_ACTIVE_SESSIONS = weakref.WeakSet()
_JUMPTABLE_CACHE_CLEARS = 0


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


def _compare(api, baseline, candidate, blob, inventory, input_digests):
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
                                 TOOLS / 'gcc3_search_data.py', TOOLS / 'toolchain/jumptable.py',
                                 TOOLS / 'toolchain/byteident.py',
                                 TOOLS / 'toolchain/induction_jumptable.py',
                                 TOOLS / 'toolchain/readonly_copy.py')},
            'inputs': {key: {'path': path, 'sha256': input_digests[path]} for key, path in
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
            'allocated_data_relocation_audit': data_audit.compare(baseline, candidate),
            'scope': 'Grades cover only functions shared by all three objects. Missing/added functions '
                     'are reported separately. Layout compares relative address groups within sections, '
                     'not absolute placement or source definition order. No adoption decision or '
                     'compiler/differential provenance is inferred.'}


class ComparisonSession:
    """Reuse object analysis safely across a batch of variant comparisons.

    The ELF analyzers cache by pathname. A session hashes every input before
    analysis and clears all pathname-keyed byteident and jumptable caches if a
    previously seen path changes content. The pathname registry and lock are
    shared across sessions because jumptable caches are module-global. Size
    inventories use content hashes, so identical objects at different paths
    share an ``nm`` result. Restart Python after changing an analysis tool.
    """

    _TOOL_INPUTS = (
        Path(__file__), TOOLS / 'gcc3_search_pilot.py',
        TOOLS / 'gcc3_search_data.py', TOOLS / 'toolchain/jumptable.py',
        TOOLS / 'toolchain/byteident.py', TOOLS / 'toolchain/induction_jumptable.py',
        TOOLS / 'toolchain/readonly_copy.py',
    )

    def __init__(self, blob=None):
        sys.path.insert(0, str(TOOLS / 'toolchain'))
        self._api = pilot.load_module('gcc3_compare_byteident', TOOLS / 'toolchain/byteident.py')
        self.blob = str(Path(blob or self._api.BLOB).resolve())
        self._sizes_by_content = {}
        self._size_hits = 0
        self._size_misses = 0
        self._invalidations = 0
        self._comparison_count = 0
        self._lock = threading.RLock()
        self._jumptable = data_audit.jumptable
        self._tool_digests = self._tool_snapshot()
        if self._tool_digests != _PROCESS_TOOL_DIGESTS:
            raise ValueError('analysis tool changed since import; restart Python before creating a session')
        with _GLOBAL_COMPARISON_LOCK:
            _ACTIVE_SESSIONS.add(self)

    @staticmethod
    def _digest(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()

    def _tool_snapshot(self):
        return {str(Path(path).resolve()): self._digest(path) for path in self._TOOL_INPUTS}

    def _check_tools(self):
        if self._tool_snapshot() != self._tool_digests:
            raise ValueError('analysis tool changed during comparison session; restart Python')

    def _snapshot_inputs(self, paths):
        digests = {}
        changed = False
        unregistered = False
        for path in paths:
            if not Path(path).is_file():
                raise ValueError('missing object: ' + path)
            digest = self._digest(path)
            digests[path] = digest
            if path not in _GLOBAL_PATH_DIGESTS:
                unregistered = True
            elif _GLOBAL_PATH_DIGESTS[path] != digest:
                changed = True
        if changed:
            _invalidate_all_path_caches()
        elif unregistered:
            # A caller may have populated jumptable's module-global LRUs before
            # this session had a digest for the path. Their age/content is
            # unknowable, so discard only those shared caches on first sight.
            _clear_jumptable_caches()
        _GLOBAL_PATH_DIGESTS.update(digests)
        return digests

    def _sizes(self, path, digest):
        if digest in self._sizes_by_content:
            self._size_hits += 1
            return self._sizes_by_content[digest]
        result = self._api.sizes(path)
        if self._digest(path) != digest:
            raise ValueError('object changed during comparison: ' + path)
        self._sizes_by_content[digest] = result
        self._size_misses += 1
        return result

    def _assert_inputs_stable(self, digests):
        for path, before in digests.items():
            try:
                after = self._digest(path)
            except OSError as exc:
                raise ValueError('object changed during comparison: ' + path) from exc
            if after != before:
                raise ValueError('object changed during comparison: ' + path)

    def compare(self, baseline, candidate, blob=None):
        with _GLOBAL_COMPARISON_LOCK, self._lock:
            self._check_tools()
            baseline = str(Path(baseline).resolve())
            candidate = str(Path(candidate).resolve())
            reference = str(Path(blob or self.blob).resolve())
            paths = (reference, baseline, candidate)
            try:
                input_digests = self._snapshot_inputs(paths)
            except (OSError, ValueError):
                _invalidate_after_input_change({path: _GLOBAL_PATH_DIGESTS[path]
                                                for path in paths if path in _GLOBAL_PATH_DIGESTS})
                raise
            try:
                inventory = {key: self._sizes(path, input_digests[path]) for key, path in
                             [('reference', reference), ('baseline', baseline), ('candidate', candidate)]}
                result = _compare(self._api, baseline, candidate, reference, inventory, input_digests)
            finally:
                try:
                    self._assert_inputs_stable(input_digests)
                except (OSError, ValueError):
                    _invalidate_after_input_change(input_digests)
                    raise
                self._check_tools()
            self._comparison_count += 1
            return result

    def compare_many(self, cases, blob=None):
        """Compare an iterable of ``(baseline, candidate)`` path pairs."""
        return [self.compare(baseline, candidate, blob) for baseline, candidate in cases]

    def cache_info(self):
        with _GLOBAL_COMPARISON_LOCK, self._lock:
            return {'comparisons': self._comparison_count,
                    'path_content_invalidations': self._invalidations,
                    'jumptable_cache_clears': _JUMPTABLE_CACHE_CLEARS,
                    'size_inventory_hits': self._size_hits,
                    'size_inventory_misses': self._size_misses,
                    'byteident': {name: getattr(self._api, name).cache_info()._asdict()
                                  for name in ('section_symbols', 'function_owners', 'body')},
                    'jumptable': {name: getattr(self._jumptable, name).cache_info()._asdict()
                                  for name in ('instructions', 'section_direct_edges')}}

    def _clear_path_caches(self):
        for name in ('section_symbols', 'function_owners', 'body'):
            getattr(self._api, name).cache_clear()
        self._invalidations += 1

    def _discard_size_inventories(self, digests):
        for digest in digests:
            self._sizes_by_content.pop(digest, None)


_PROCESS_TOOL_DIGESTS = {
    str(Path(path).resolve()): hashlib.sha256(Path(path).read_bytes()).hexdigest()
    for path in ComparisonSession._TOOL_INPUTS
}


def _invalidate_all_path_caches():
    for session in list(_ACTIVE_SESSIONS):
        session._clear_path_caches()
    _clear_jumptable_caches()


def _clear_jumptable_caches():
    global _JUMPTABLE_CACHE_CLEARS
    jumptable = data_audit.jumptable
    for name in ('instructions', 'section_direct_edges'):
        getattr(jumptable, name).cache_clear()
    _JUMPTABLE_CACHE_CLEARS += 1


def _invalidate_after_input_change(digests):
    _invalidate_all_path_caches()
    for session in list(_ACTIVE_SESSIONS):
        session._discard_size_inventories(digests.values())
    for path in digests:
        _GLOBAL_PATH_DIGESTS.pop(path, None)


def compare(baseline, candidate, blob=None, session=None):
    """Compare one pair, or reuse ``session`` as part of a larger batch."""
    if session is None:
        session = ComparisonSession(blob)
    return session.compare(baseline, candidate, blob)


def compare_many(cases, blob=None):
    """Compare path pairs with one persistent analysis session."""
    return ComparisonSession(blob).compare_many(cases)


def _write_report(session, baseline, candidate, blob, output):
    stream = None
    try:
        stream = output.open('x')
        result = session.compare(baseline, candidate, blob)
        json.dump(result, stream, indent=2, sort_keys=True)
        stream.write('\n')
        print('valid: %s; %d shared functions; EXACT +%d/-%d, REGALLOC +%d/-%d; emitted order matches reference: %s' %
              (output, result['shared_sized_functions'],
               len(result['grade_changes']['EXACT']['gained']),
               len(result['grade_changes']['EXACT']['lost']),
               len(result['grade_changes']['REGALLOC']['gained']),
               len(result['grade_changes']['REGALLOC']['lost']),
               result['layout']['candidate_matches_reference']))
        return 0
    except (OSError, ValueError, RuntimeError, ELFError, subprocess.SubprocessError) as exc:
        if stream is not None:
            stream.seek(0)
            stream.truncate()
            json.dump({'status': 'invalid', 'shared_sized_functions': None, 'error': str(exc)}, stream)
            stream.write('\n')
        print('invalid: %s; denominator not established: %s' % (output, exc), file=sys.stderr)
        return 1
    finally:
        if stream is not None:
            stream.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True)
    parser.add_argument('--candidate', action='append', required=True,
                        help='candidate object; repeat with --output-dir for a cached batch')
    parser.add_argument('--blob')
    outputs = parser.add_mutually_exclusive_group(required=True)
    outputs.add_argument('--output', type=Path, help='single-candidate report; never overwrites')
    outputs.add_argument('--output-dir', type=Path,
                         help='batch reports, one file per candidate; existing files are never overwritten')
    args = parser.parse_args(argv)
    if args.output is not None and len(args.candidate) != 1:
        parser.error('--output requires exactly one --candidate')
    try:
        session = ComparisonSession(args.blob)
    except (OSError, ValueError, RuntimeError) as exc:
        print('invalid: analysis session could not start: ' + str(exc), file=sys.stderr)
        return 1
    if args.output is not None:
        return _write_report(session, args.baseline, args.candidate[0], args.blob, args.output)

    try:
        args.output_dir.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        print('invalid: batch output directory: ' + str(exc), file=sys.stderr)
        return 1
    status = 0
    for index, candidate in enumerate(args.candidate, 1):
        output = args.output_dir / ('%04d-%s.json' % (index, Path(candidate).name))
        status |= _write_report(session, args.baseline, candidate, args.blob, output)
    return status


if __name__ == '__main__':
    raise SystemExit(main())
