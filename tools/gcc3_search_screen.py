#!/usr/bin/env python3
"""Screen one completed GCC scratch capture against its unchanged TU baseline.

The 53 incoming cursors are frozen-input diagnostics, not source-realizable
orders or predicted object matches. Unsupported events disable whole-stream
modeling; their complete census remains useful coverage evidence.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

TOOLS = Path(__file__).resolve().parent
# tools/dis.py must not shadow stdlib dis when byteident imports its helpers.
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
import dis  # noqa: F401 -- initialize the standard-library module first
sys.path.insert(0, str(TOOLS))
import gcc3_search_capture as capture_api
import gcc3_search_model as model_api

SCHEMA = 'gcc3-scratch-screen-v1'


class ScreenError(ValueError):
    """Invalid or stale measurement; no screen conclusions are available."""


def require(condition, message):
    if not condition:
        raise ScreenError(message)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def checked_hash(path, expected, label):
    require(Path(path).is_file(), label + ': missing file ' + str(path))
    actual = sha(path)
    require(actual == expected, label + ': hash mismatch ' + str(path))
    return actual


def common_event_shape(event, where):
    """Validate observations without assuming a mode's register-width semantics."""
    require(isinstance(event, dict), where + ': expected event object')
    required = model_api.INPUT_KEYS + ('cursor_before', 'cursor_after', 'selected', 'candidates')
    require(set(required) <= event.keys(), where + ': missing common event fields')
    for key in ('function', 'mode', 'constraint'):
        require(isinstance(event[key], str) and bool(event[key]), where + ': invalid ' + key)
    for key in ('cursor_before', 'cursor_after'):
        model_api._integer(event[key], 0, model_api.SLOTS - 1, where + '.' + key)
    if event['selected'] is not None:
        model_api._integer(event['selected'], 0, model_api.SLOTS - 1, where + '.selected')
    model_api._integer(event['from'], 0, 4, where + '.from')
    model_api._integer(event['to'], event['from'], 4, where + '.to')
    allocation = event['allocation_order']
    require(isinstance(allocation, list) and len(allocation) == model_api.SLOTS,
            where + ': expected 53 allocation slots')
    for value in allocation:
        model_api._integer(value, 0, model_api.SLOTS - 1, where + '.allocation_order')
    model_api._registers(event['excluded'], where + '.excluded')
    state = event['eligibility']
    fields = {'class', 'class_members', 'live', 'fixed', 'call_used', 'ever_live',
              'reload_completed', 'frame_pointer_needed'}
    require(isinstance(state, dict) and fields <= state.keys(),
            where + ': missing mechanism inputs')
    model_api._integer(state['class'], 0, 1000, where + '.eligibility.class')
    for key in ('class_members', 'live'):
        model_api._registers(state[key], where + '.eligibility.' + key)
    for key in ('fixed', 'call_used', 'ever_live'):
        model_api._flags(state[key], where + '.eligibility.' + key)
    for key in ('reload_completed', 'frame_pointer_needed'):
        require(type(state[key]) in (int, bool) and state[key] in (0, 1),
                where + ': invalid eligibility ' + key)
    require(isinstance(event['candidates'], list) and bool(event['candidates']),
            where + ': missing candidate observations')
    for index, candidate in enumerate(event['candidates']):
        label = where + '.candidates[%d]' % index
        require(isinstance(candidate, dict) and {'order_index', 'register'} <= candidate.keys(),
                label + ': missing slot/register')
        model_api._integer(candidate['order_index'], 0, model_api.SLOTS - 1, label + '.order_index')
        model_api._integer(candidate['register'], 0, model_api.SLOTS - 1, label + '.register')
        reg = allocation[candidate['order_index']]
        require(candidate['register'] == reg, label + ': allocation slot/register mismatch')
        expected = {'live': state['live'], 'class_ok': reg in state['class_members']}
        expected.update({key: state[key][reg] for key in ('fixed', 'call_used', 'ever_live')})
        expected.update({key: state[key] for key in ('reload_completed', 'frame_pointer_needed')})
        for key, value in expected.items():
            if key in candidate:
                if key == 'live':
                    model_api._registers(candidate[key], label + '.live')
                else:
                    require(type(candidate[key]) in (int, bool) and candidate[key] in (0, 1),
                            label + ': invalid flag ' + key)
                require(candidate[key] == value, label + ': changed ' + key)


def screen_trace(trace):
    """Census every event; locally check supported choices without composition."""
    require(isinstance(trace, dict) and {'events', 'errors', 'exit_codes', 'pending'} <= trace.keys(),
            'trace: missing envelope fields')
    require(trace['errors'] == [] and trace['exit_codes'] == [0] and trace['pending'] == 0,
            'incomplete compiler observation')
    require(type(trace['pending']) is int and type(trace['exit_codes'][0]) is int,
            'trace envelope uses non-integer pending/exit code')
    require(isinstance(trace['events'], list) and bool(trace['events']), 'trace: empty event stream')
    unsupported = []
    locally_checked = 0
    for index, event in enumerate(trace['events']):
        where = 'event[%d]' % index
        common_event_shape(event, where)
        try:
            eligible = model_api._event(event, where)
        except model_api.ModelError as exc:
            # Malformed evidence is a hard failure, not a model extension request.
            if 'unsupported' not in str(exc):
                raise ScreenError(str(exc)) from exc
            unsupported.append({'index': index, 'function': event['function'],
                                'mode': event['mode'], 'constraint': event['constraint'],
                                'reason': str(exc)})
        else:
            # Each check starts from its observed cursor. This does not carry
            # modeled state through an unsupported predecessor or predict it.
            predicted = model_api._transfer(event, event['cursor_before'], eligible)
            require(predicted == (event['selected'], event['cursor_after']),
                    where + ': selection/cursor mismatch in local supported decision')
            locally_checked += 1
    capture_api.validate_trace(trace)
    if unsupported:
        return {'status': 'unsupported_event_census',
                'observed_events': len(trace['events']),
                'supported_events': len(trace['events']) - len(unsupported),
                'locally_checked_supported_decisions': locally_checked,
                'unsupported_events': unsupported,
                'modeled_events': 0,
                'scope': 'No supported prefix or cross-event cursor predictions.'}, None
    calibrated = model_api.calibrate(trace)
    return {'status': 'calibrated_whole_stream',
            'observed_events': len(trace['events']),
            'supported_events': len(trace['events']), 'unsupported_events': [],
            'locally_checked_supported_decisions': locally_checked,
            'modeled_events': calibrated['counts']['modeled_events'],
            'blocks': calibrated['counts']['blocks']}, calibrated


def block_sensitivity(block):
    """Read existing all-cursor transition tables; do not rebuild the model."""
    signatures = defaultdict(list)
    for transition in block['transitions']:
        signature = tuple(row['selected'] for row in transition['events'])
        signatures[signature].append(transition['start'])
    return {'block': block['id'], 'incoming_cursor_states': model_api.SLOTS,
            'distinct_choice_signatures': len(signatures),
            'sensitive': len(signatures) > 1,
            'signatures': [{'selected': list(signature), 'starts': starts}
                           for signature, starts in signatures.items()]}


def classify_functions(grades, events, calibrated=None, defined_functions=None):
    """Join canonical verdicts to assembler-name events without dropping misses."""
    require(bool(grades), 'empty full-TU shared-sized-function denominator (0)')
    groups = defaultdict(list)
    for event in events:
        groups[event['function']].append(event)
    blocks = defaultdict(list)
    if calibrated is not None:
        for block in calibrated['blocks']:
            blocks[block['function']].append(block_sensitivity(block))
    rows = []
    for name, grade in sorted(grades.items()):
        observed = groups[name]
        rows.append({'symbol': name, 'verdict': list(grade), 'grade': grade[0],
                     'observed_scratch_events': len(observed),
                     'modes': dict(sorted(Counter(e['mode'] for e in observed).items())),
                     'constraints': dict(sorted(Counter(e['constraint'] for e in observed).items())),
                     'cursor_diagnostic': ('no_observed_events' if not observed else
                                           'all_incoming_cursors' if calibrated is not None else
                                           'unavailable_whole_stream_unsupported'),
                     'blocks': blocks[name]})
    nonexact = [row for row in rows if row['grade'] != 'EXACT']
    known = set(grades) if defined_functions is None else set(defined_functions)
    return {'shared_sized_functions': len(rows),
            'grade_counts': dict(sorted(Counter(row['grade'] for row in rows).items())),
            'nonexact_functions': len(nonexact),
            'nonexact_with_observed_events': [r['symbol'] for r in nonexact if r['observed_scratch_events']],
            'nonexact_without_observed_events': [r['symbol'] for r in nonexact if not r['observed_scratch_events']],
            'nonexact_cursor_sensitive': ([r['symbol'] for r in nonexact
                                           if any(b['sensitive'] for b in r['blocks'])]
                                          if calibrated is not None else None),
            'cursor_diagnostic_nonexact_function_denominator':
                sum(bool(r['observed_scratch_events']) for r in nonexact) if calibrated is not None else 0,
            'event_functions_without_shared_blob_function': sorted(set(groups) - set(grades)),
            'event_functions_without_sized_definition': sorted(set(groups) - known),
            'functions': rows}


def validate_capture(path, baseline_object, baseline_config=None):
    path, baseline_object = Path(path).resolve(), Path(baseline_object).resolve()
    result = json.loads((path / 'results.json').read_text())
    require(result.get('schema') == 1 and result.get('status') == 'valid',
            'capture is not a completed valid schema-1 run')
    require(result['object'] == 'initial/candidate.o' and result['trace'] == 'scratch-observe.json',
            'unexpected capture artifact paths')
    require(all(result['checks'].get(key) is True for key in
                ('repeated_object_equal', 'initial_plain_observed_assembly_equal', 'observed_object_equal')),
            'capture lacks unchanged-observation controls')
    require(all(command['returncode'] == 0 for command in result['commands']),
            'capture records a failed command')
    root, source = Path(result['root']), Path(result['source'])
    checked_hash(TOOLS / 'gcc3_search_capture.py', result['capture_sha256'], 'capture apparatus')
    checked_hash(TOOLS / 'gcc3_search_capture_gdb.py', result['observer_sha256'], 'observer apparatus')
    checked_hash(source, result['source_sha256'], 'current source')
    checked_hash(path / ('source-input' + source.suffix), result['source_sha256'], 'saved source')
    checked_hash(Path(result['config_path']), result['config_sha256'], 'current capture configuration')
    checked_hash(path / '.build-config', result['config_sha256'], 'saved capture configuration')
    require((path / '.build-config').read_text() == result['config'], 'configuration text mismatch')
    for name, digest in result['header_sha256'].items():
        checked_hash(root / name, digest, 'current repository header')
    tracked = subprocess.run(['git', '-C', str(root), 'ls-files', '-z', '--',
                              'include', 'src', 'tools/toolchain/period_compat.h'],
                             check=True, capture_output=True, text=True).stdout.split('\0')
    headers = {p for p in tracked if p and
               (Path(p).suffix in ('.h', '.hpp', '.hh', '.hxx') or p.startswith('include/'))}
    require(headers == set(result['header_sha256']), 'current tracked header inventory changed')
    parent_headers = {str(p): sha(p) for p in sorted(source.parent.iterdir())
                      if p.is_file() and p.suffix in ('.h', '.hpp', '.hh', '.hxx')}
    require(parent_headers == result['source_parent_header_sha256'], 'current source-parent headers changed')
    kind = result['compiler_kind']
    require(kind in capture_api.SUPPORTED and result['compiler_sha256'] == capture_api.SUPPORTED[kind],
            'unsupported compiler identity')
    checked_hash(path / kind, result['compiler_sha256'], 'captured compiler executable')
    require((source.suffix == '.cpp') == (kind == 'cc1plus'), 'source/compiler kind mismatch')
    for name in (result['object'], 'repeat/candidate.o', 'observed.o'):
        checked_hash(path / name, result['object_sha256'], 'captured object')
    checked_hash(baseline_object, result['object_sha256'], 'raw unchanged baseline object')
    preprocessed = list((path / 'initial').glob('*.ii' if kind == 'cc1plus' else '*.i'))
    assembly = list((path / 'initial').glob('*.s'))
    require(len(preprocessed) == len(assembly) == 1, 'ambiguous saved compiler input/assembly')
    for name in (preprocessed[0], path / preprocessed[0].name):
        checked_hash(name, result['preprocessed_sha256'], 'saved preprocessed input')
    for name in (assembly[0], path / 'plain.s', path / 'observed.s'):
        checked_hash(name, result['assembly_sha256'], 'saved assembly')
    trace_path = path / result['trace']
    trace = json.loads(trace_path.read_text())
    provenance = {key: result[key] for key in ('compiler_sha256', 'image_id', 'effective_flags',
                                              'observer_sha256', 'capture_sha256')}
    require(trace.get('provenance') == provenance, 'trace/capture provenance mismatch')
    require('-DDSPLIB_REPRODUCE_BUGS' in result['effective_flags'], 'missing bug-reproduction flag')
    capture_api.validate_trace(trace)
    require(len(trace['events']) == result['events'] and
            len({e['function'] for e in trace['events']}) == result['functions_with_events'],
            'trace event/function denominator mismatch')
    if baseline_config is None:
        baseline_config = next((p / '.build-config' for p in baseline_object.parents
                                if (p / '.build-config').is_file()), None)
    if baseline_config is not None:
        checked_hash(Path(baseline_config), result['config_sha256'], 'baseline configuration')
    return result, trace, {'capture': str(path), 'results_sha256': sha(path / 'results.json'),
                           'trace_sha256': sha(trace_path), 'source': str(source),
                           'source_sha256': result['source_sha256'],
                           'baseline_object': str(baseline_object), 'object_sha256': result['object_sha256'],
                           'raw_baseline_object_equal': True,
                           'baseline_config': str(baseline_config) if baseline_config is not None else None,
                           'baseline_config_checked': baseline_config is not None,
                           'header_files_checked': len(result['header_sha256']),
                           'provenance': provenance,
                           'trace_integrity_scope': 'Capture schema 1 has no retained trace digest; '
                           'this report pins the analyzed trace. Provenance, envelope and supported '
                           'whole-stream decisions are checked independently.'}


def load_byteident():
    sys.path.insert(0, str(TOOLS / 'toolchain'))
    spec = importlib.util.spec_from_file_location('gcc3_screen_byteident', TOOLS / 'toolchain/byteident.py')
    api = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(api)
    return api


def analyze(capture_path, baseline_object, blob=None, baseline_config=None):
    """Return a fresh JSON-ready report or raise on invalid/stale evidence."""
    result, trace, identity = validate_capture(capture_path, baseline_object, baseline_config)
    model_status, calibrated = screen_trace(trace)
    api = load_byteident()
    blob = Path(api.BLOB if blob is None else blob).resolve()
    require(blob.is_file(), 'missing reference object: ' + str(blob))
    candidate = str(Path(capture_path).resolve() / result['object'])
    ours, original = api.sizes(candidate), api.sizes(str(blob))
    names = sorted(ours.keys() & original.keys())
    require(bool(names), 'empty full-TU shared-sized-function denominator (0)')
    grades = {name: list(api.verdict(*api.body(str(blob), name), *api.body(candidate, name)))
              for name in names}
    functions = classify_functions(grades, trace['events'], calibrated, ours)
    for row in functions['functions']:
        row.update({'candidate_size': ours[row['symbol']], 'blob_size': original[row['symbol']]})
    functions['defined_sized_functions'] = sorted(ours)
    functions['defined_sized_functions_without_blob_function'] = sorted(ours.keys() - original.keys())
    return {'schema': SCHEMA, 'status': 'valid', 'identity': identity,
            'screen_sha256': sha(Path(__file__)),
            'model_sha256': sha(TOOLS / 'gcc3_search_model.py'),
            'byteident_sha256': sha(TOOLS / 'toolchain/byteident.py'),
            'blob': str(blob), 'blob_sha256': sha(blob),
            'mode_counts': dict(sorted(Counter(e['mode'] for e in trace['events']).items())),
            'model': model_status, 'coverage': functions,
            'interpretation': 'All-cursor signatures hold event inputs fixed and vary the incoming '
            'cursor of each target block independently. The 53 states are diagnostics, not '
            'source-realizable configurations, object-match predictions or recovered-source evidence. '
            'No observed events means this mechanism was not observed, not that a function is invariant.'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--baseline-object', type=Path, required=True)
    parser.add_argument('--baseline-config', type=Path)
    parser.add_argument('--blob', type=Path)
    parser.add_argument('--output', type=Path, required=True, help='fresh JSON file outside capture')
    args = parser.parse_args(argv)
    stream = None
    try:
        require(not args.output.resolve().is_relative_to(args.capture.resolve()),
                'output must be outside the read-only capture directory')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        stream = args.output.open('x')
        report = analyze(args.capture, args.baseline_object, args.blob, args.baseline_config)
        json.dump(report, stream, indent=2, sort_keys=True)
        stream.write('\n')
        coverage = report['coverage']
        print('valid: shared sized functions=%d, nonexact=%d, with events=%d, without events=%d; %s' %
              (coverage['shared_sized_functions'], coverage['nonexact_functions'],
               len(coverage['nonexact_with_observed_events']),
               len(coverage['nonexact_without_observed_events']), report['model']['status']))
        return 0
    except (OSError, ValueError, RuntimeError, KeyError, TypeError, subprocess.SubprocessError) as exc:
        if stream is not None:
            stream.seek(0)
            stream.truncate()
            json.dump({'schema': SCHEMA, 'status': 'invalid', 'error': str(exc),
                       'shared_sized_functions': None, 'denominator_status': 'not established'}, stream, indent=2)
            stream.write('\n')
        print('invalid: shared sized functions=unestablished; ' + str(exc), file=sys.stderr)
        return 1
    finally:
        if stream is not None:
            stream.close()


if __name__ == '__main__':
    sys.exit(main())
