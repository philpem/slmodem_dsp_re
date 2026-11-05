#!/usr/bin/env python3
"""Bounded full-TU source-order controls for the GCC 3 scratch-state model.

All generated sources and objects live in a fresh --output directory. The
six orders are diagnostic controls, never proposed source recovery. Predictions
are frozen before any held-out compilation; a changed event domain invalidates
the model for that cell rather than teaching it the observed answer.
"""

import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import sys
import time

# Avoid tools/dis.py shadowing Python's dis while pyelftools imports inspect.
TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
from elftools.elf.elffile import ELFFile

SOURCE = Path('src/pump/v90/V90Phase4Modulator.cpp')
METHODS = ('resetBeforRRN', 'resetRRNSecondSection',
           'recivedPartTwoSilenceRrnSUV')


class PilotError(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise PilotError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')
    temporary.replace(path)


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def definitions(text):
    """Extract exactly guarded signatures, ignoring braces in comments/strings."""
    found = {}
    for method in METHODS:
        signature = 'void\nV90Phase4Modulator::' + method + '()\n{'
        require(text.count(signature) == 1, 'signature not unique: ' + method)
        start = text.index(signature)
        opening = start + len(signature) - 1
        depth, state, index = 0, 'code', opening
        while index < len(text):
            c = text[index]
            following = text[index:index + 2]
            if state == 'line':
                if c == '\n':
                    state = 'code'
            elif state == 'block':
                if following == '*/':
                    state = 'code'
                    index += 1
            elif state in ('"', "'"):
                if c == '\\':
                    index += 1
                elif c == state:
                    state = 'code'
            elif following == '//':
                state = 'line'
                index += 1
            elif following == '/*':
                state = 'block'
                index += 1
            elif c in ('"', "'"):
                state = c
            elif c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    found[method] = (start, index + 1, text[start:index + 1])
                    break
            index += 1
        require(method in found, 'unterminated definition: ' + method)
    slots = sorted(found, key=lambda name: found[name][0])
    require(all(found[a][1] <= found[b][0] for a, b in zip(slots, slots[1:])),
            'function extraction overlaps')
    return found, slots


def permute_source(text, found, slots, order):
    require(sorted(order) == sorted(slots), 'invalid source permutation')
    pieces, offset = [], 0
    for slot, method in zip(slots, order):
        start, end, _ = found[slot]
        pieces.extend((text[offset:start], found[method][2]))
        offset = end
    pieces.append(text[offset:])
    result = ''.join(pieces)
    extracted, actual = definitions(result)
    require(actual == list(order), 'source generator did not produce requested order')
    require(all(extracted[n][2] == found[n][2] for n in slots),
            'source generator changed a function definition')
    return result


def run_command(argv, output, label):
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    start = time.monotonic()
    log = output / (label + '.log')
    with log.open('w') as stream:
        completed = subprocess.run(argv, cwd=ROOT, stdout=stream,
                                   stderr=subprocess.STDOUT, check=False)
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    return {'argv': list(map(str, argv)), 'cwd': str(ROOT), 'log': str(log),
            'returncode': completed.returncode,
            'wall_seconds': time.monotonic() - start,
            'child_user_seconds': after.ru_utime - before.ru_utime,
            'child_system_seconds': after.ru_stime - before.ru_stime,
            'cpu_scope': 'local subprocess children; excludes Docker guest CPU'}


def measured(function, *args, **kwargs):
    wall, cpu = time.monotonic(), time.process_time()
    value = function(*args, **kwargs)
    return value, {'wall_seconds': time.monotonic() - wall,
                   'process_cpu_seconds': time.process_time() - cpu}


def exported_bindings(path):
    with Path(path).open('rb') as stream:
        table = ELFFile(stream).get_section_by_name('.symtab')
        require(table is not None, 'object has no symbol table: ' + str(path))
        return sorted([{'name': symbol.name, 'binding': symbol['st_info']['bind'],
                        'visibility': symbol['st_other']['visibility'],
                        'type': symbol['st_info']['type']}
                       for symbol in table.iter_symbols()
                       if symbol['st_shndx'] != 'SHN_UNDEF'
                       and symbol['st_info']['bind'] in ('STB_GLOBAL', 'STB_WEAK')],
                      key=lambda row: (row['name'], row['binding'], row['type']))


def allocated_data(path):
    with Path(path).open('rb') as stream:
        elf = ELFFile(stream)
        return [{'section': section.name, 'type': section['sh_type'],
                 'size': section['sh_size'],
                 'sha256': hashlib.sha256(section.data()).hexdigest()}
                for section in elf.iter_sections()
                if section['sh_flags'] & 2 and not section['sh_flags'] & 4]


def object_report(byteident, candidate, baseline=None):
    path, blob = str(candidate), str(byteident.BLOB)
    ours, original = byteident.sizes(path), byteident.sizes(blob)
    names = sorted(ours.keys() & original.keys())
    require(names, 'empty full-TU shared-function denominator')
    grades = {name: list(byteident.verdict(*byteident.body(blob, name),
                                          *byteident.body(path, name)))
              for name in names}
    result = {'shared_sized_functions': len(names), 'grades': grades,
              'exact_symbols': [n for n in names if grades[n][0] == 'EXACT'],
              'defined_sized_functions': sorted(ours),
              'no_blob_function': sorted(ours.keys() - original.keys()),
              'allocated_data_raw_sections': allocated_data(path),
              'exported_bindings': exported_bindings(path)}
    if baseline is not None:
        oldpath = str(baseline)
        old = byteident.sizes(oldpath)
        shared = sorted(old.keys() & ours.keys())
        oldexact = {n for n in old.keys() & original.keys()
                    if byteident.verdict(*byteident.body(blob, n),
                                         *byteident.body(oldpath, n))[0] == 'EXACT'}
        nowexact = set(result['exact_symbols'])
        result.update({'gains': sorted(nowexact - oldexact),
                       'losses': sorted(oldexact - nowexact),
                       'missing_functions': sorted(old.keys() - ours.keys()),
                       'new_functions': sorted(ours.keys() - old.keys()),
                       'baseline_shared_functions': len(shared),
                       'changed_bodies_or_relocations': [n for n in shared
                           if byteident.body(oldpath, n) != byteident.body(path, n)],
                       'exported_bindings_unchanged': exported_bindings(oldpath)
                           == result['exported_bindings'],
                       'allocated_data_raw_sections_unchanged': allocated_data(oldpath)
                           == result['allocated_data_raw_sections']})
    return result


def capture(source, directory, config, ledger, output):
    command = run_command([sys.executable, str(TOOLS / 'gcc3_search_capture.py'),
                           '--source', str(source), '--output', str(directory),
                           '--config', str(config)], output, directory.name + '-capture')
    ledger['commands'].append(command)
    write_json(output / 'results.json', ledger)
    require(command['returncode'] == 0,
            'capture failed (%s); see %s' % (command['returncode'], command['log']))
    manifest = json.loads((directory / 'results.json').read_text())
    require(manifest['status'] == 'valid', 'capture manifest is not valid')
    trace = json.loads((directory / 'scratch-observe.json').read_text())
    require(trace['events'] and not trace['errors'] and not trace['pending']
            and trace['exit_codes'] == [0], 'invalid/empty captured trace')
    ledger['denominator']['compiled_cells'] += 1
    return trace, manifest, directory / 'initial/candidate.o'


def event_blocks(trace):
    """Use the observer's contiguous function occurrences as the model does."""
    import itertools
    occurrences, result = {}, {}
    for name, group in itertools.groupby(trace['events'], lambda event: event['function']):
        occurrences[name] = occurrences.get(name, 0) + 1
        result[name + '#' + str(occurrences[name])] = list(group)
    return result


def target_events(trace, selected_blocks):
    blocks = event_blocks(trace)
    return [{'block': block, 'event': index, 'selected': event['selected']}
            for block in selected_blocks.values()
            for index, event in enumerate(blocks[block]) if event['selected'] is not None]


def actual_matches(trace, targets):
    blocks = event_blocks(trace)
    return all(target['block'] in blocks
               and target['event'] < len(blocks[target['block']])
               and blocks[target['block']][target['event']]['selected'] == target['selected']
               for target in targets)


def predicted_matches(prediction, targets):
    blocks = {block['id']: block['events'] for block in prediction['blocks']}
    return all(blocks[target['block']][target['event']]['selected'] == target['selected']
               for target in targets)


def negative_controls(api, model, trace, ledger, output):
    """Show actual outcome corruption and valid changed eligibility are refused."""
    rows = []
    for label in ('corrupt-selected-outcome', 'changed-unused-eligibility'):
        changed = copy.deepcopy(trace)
        event = changed['events'][0]
        if label == 'corrupt-selected-outcome':
            event['selected'] = None if event['selected'] is not None else 0
        else:
            # Register 52 is outside the supported SI/general class. Changing
            # its fixed flag leaves the actual observed transfer valid, while
            # changing a frozen input that verify must explicitly refuse.
            require(52 not in event['eligibility']['class_members'],
                    'negative control needs a non-class register')
            event['eligibility']['fixed'][52] ^= 1
            for candidate in event.get('candidates', []):
                if candidate['register'] == 52 and 'fixed' in candidate:
                    candidate['fixed'] = event['eligibility']['fixed'][52]
            api.calibrate(changed)  # prove this is a valid changed-input trace
        write_json(output / (label + '.json'), changed)
        try:
            api.verify(model, model['baseline_order'], changed)
        except api.ModelError as error:
            rows.append({'label': label, 'refused': True, 'error': str(error)})
        else:
            raise PilotError('negative control silently accepted: ' + label)
    ledger['negative_controls'] = rows
    ledger['denominator']['negative_controls_refused'] = len(rows)
    write_json(output / 'results.json', ledger)


def run_pilot(output, ledger):
    # Import byteident's sibling helpers only after pyelftools is initialized.
    sys.path.insert(0, str(TOOLS / 'toolchain'))
    byteident = load_module('gcc3_pilot_byteident', TOOLS / 'toolchain/byteident.py')
    model_api = load_module('gcc3_search_model', TOOLS / 'gcc3_search_model.py')
    source = ROOT / SOURCE
    text = source.read_text()
    found, slots = definitions(text)
    ledger['inputs'] = {'source': str(SOURCE), 'source_sha256': digest(source),
                        'period_makefile_sha256': digest(TOOLS / 'toolchain/period.mk'),
                        'tool_sha256': {str(path.relative_to(ROOT)): digest(path) for path in
                            (TOOLS / 'gcc3_search_pilot.py', TOOLS / 'gcc3_search_model.py',
                             TOOLS / 'gcc3_search_capture.py',
                             TOOLS / 'gcc3_search_capture_gdb.py')},
                        'blob': str(byteident.BLOB), 'blob_sha256': digest(byteident.BLOB)}
    ledger['hypothesis'] = ('For these three complete function-definition swaps, '
        'observed event blocks and eligibility remain invariant and the calibrated '
        'scratch-state transition predicts every selected register. Any event-order, '
        'eligibility or selected-register disagreement falsifies that cell.')
    ledger['scope'] = ('3! permutations of three existing definitions in a complete TU; '
        'other source bytes preserved; calibration controls only; no source recovery claim')
    ledger['differential_status'] = 'NOT RUN: no retained reconstruction source change'
    ledger['partial_link_status'] = 'NOT RUN: bounded full-TU apparatus pilot'
    production = output / 'production'
    target = production / (str(SOURCE).replace('/', '_') + '.o')
    # The period makefile expands its three source-query RHSs even when the
    # resulting variables are overridden. This one-TU domain supplies all
    # lists explicitly; suppress only those unused recursive queries. The
    # authoritative period compilation recipe and flags remain unchanged.
    command = run_command(['make', '-f', 'tools/toolchain/period.mk',
                           'TC_OUT=' + str(production), 'TC_SRC=',
                           'TC_CXXSRC=' + str(SOURCE), 'TC_ASRC=', 'MAKE=/bin/true',
                           str(target)],
                          output, 'production')
    ledger['commands'].append(command)
    write_json(output / 'results.json', ledger)
    require(command['returncode'] == 0 and target.is_file(),
            'fresh isolated production baseline failed; see ' + command['log'])
    config = production / '.build-config'
    ledger['inputs'].update({'config': config.read_text(), 'config_sha256': digest(config),
                             'production_object_sha256': digest(target)})
    inputs = output / 'sources'
    inputs.mkdir()
    control = inputs / 'baseline' / source.name
    control.parent.mkdir()
    control.write_text(text)
    trace, manifest, baseline_object = capture(control, output / 'baseline', config,
                                               ledger, output)
    ledger['calibration_capture'] = manifest
    ledger['baseline_raw_equal'] = target.read_bytes() == baseline_object.read_bytes()
    require(ledger['baseline_raw_equal'],
            'unchanged full-TU control does not reproduce fresh production object bytes')
    baseline_score = object_report(byteident, baseline_object)
    ledger['baseline_object'] = baseline_score
    model, timing = measured(model_api.calibrate, trace)
    ledger['model_calibration_timing'] = timing
    write_json(output / 'model.json', model)
    base_order = model['baseline_order']
    blocks = {}
    for method in slots:
        mangled = '_ZN18V90Phase4Modulator%d%sEv' % (len(method), method)
        matches = [b for b in base_order if b.startswith(mangled + '#')]
        require(len(matches) == 1, 'expected one observed event block for ' + method)
        blocks[method] = matches[0]
    positions = [base_order.index(blocks[m]) for m in slots]
    require(positions == sorted(positions), 'source order differs from baseline event order')
    ledger['selected_blocks'] = blocks
    # Baseline observed selections define a reproducibility target, not a claim
    # about the unavailable original source. The search is independently checked.
    targets = target_events(trace, blocks)
    require(targets, 'no selected scratch events in chosen methods')
    family = {'groups': [[blocks[m] for m in slots]], 'targets': targets, 'limit': 6}
    ledger['target_family'] = family
    search, timing = measured(model_api.search, model, family)
    ledger['search'] = search
    ledger['search_timing'] = timing
    import itertools
    permutations = list(itertools.permutations(slots))
    permutations.remove(tuple(slots))
    permutations.insert(0, tuple(slots))
    for index, order in enumerate(permutations):
        block_order = list(base_order)
        for position, method in zip(positions, order):
            block_order[position] = blocks[method]
        prediction, timing = measured(model_api.predict, model, order=block_order)
        row = {'index': index, 'source_order': list(order), 'event_order': block_order,
               'prediction': prediction, 'model_timing': timing, 'status': 'predicted'}
        row['predicted_target_match'] = predicted_matches(prediction, targets)
        ledger['cells'].append(row)
    require(search['evaluated'] == 6 and search['exhaustive'],
            'model search did not enumerate all six declared orders')
    predicted_hits = {tuple(row['event_order']) for row in ledger['cells']
                      if row['predicted_target_match']}
    search_hits = {tuple(row['order']) for row in search['matches']}
    require(predicted_hits == search_hits, 'search and direct prediction disagree')
    # Freeze EVERY prediction before observing any held-out cell.
    write_json(output / 'predictions.json', ledger['cells'])
    write_json(output / 'results.json', ledger)
    negative_controls(model_api, model, trace, ledger, output)
    for row in ledger['cells']:
        index = row['index']
        if index == 0:
            actual, cell_manifest, obj = trace, manifest, baseline_object
        else:
            generated = permute_source(text, found, slots, row['source_order'])
            candidate = inputs / ('order-%d' % index) / source.name
            candidate.parent.mkdir()
            candidate.write_text(generated)
            require(generated != text, 'held-out source duplicates baseline')
            actual, cell_manifest, obj = capture(candidate, output / ('order-%d' % index),
                                                  config, ledger, output)
        row['capture_manifest'] = cell_manifest
        require(all(cell_manifest[key] == manifest[key] for key in
                    ('compiler_sha256', 'image_id', 'header_sha256', 'config_sha256',
                     'observer_sha256')),
                'compiler/image/header/config/observer provenance drift between captures')
        row['object_sha256'] = digest(obj)
        try:
            row['verification'] = model_api.verify(model, row['event_order'], actual)
            row['status'] = 'verified'
            ledger['denominator']['verified_cells'] += 1
        except model_api.ModelError as error:
            row['status'] = 'invalid_model_domain'
            row['model_error'] = str(error)
            ledger['denominator']['invalid_model_cells'] += 1
        row['actual_target_match'] = actual_matches(actual, targets)
        actual_blocks = event_blocks(actual)
        row['actual_target_selections'] = [
            actual_blocks[target['block']][target['event']]['selected']
            if target['block'] in actual_blocks
            and target['event'] < len(actual_blocks[target['block']]) else 'missing-event'
            for target in targets]
        row['object'] = object_report(byteident, obj, baseline_object)
        ledger['denominator']['body_verdicts'] += row['object']['shared_sized_functions']
        require(row['object']['exported_bindings_unchanged'],
                'source-order control changed exported bindings')
        require(not row['object']['missing_functions'] and not row['object']['new_functions'],
                'source-order control changed sized-function inventory')
        write_json(output / 'results.json', ledger)
        print('cell %d/6: %s; target=%s; shared functions=%d; gains=%d losses=%d' %
              (index + 1, row['status'], row['actual_target_match'],
               row['object']['shared_sized_functions'], len(row['object']['gains']),
               len(row['object']['losses'])), flush=True)
    ledger['denominator']['actual_target_matches'] = sum(
        row['actual_target_match'] for row in ledger['cells'])
    ledger['denominator']['predicted_target_matches'] = len(predicted_hits)
    ledger['denominator']['verified_target_matches'] = sum(
        row['actual_target_match'] and row['status'] == 'verified' for row in ledger['cells'])
    ledger['denominator']['distinct_prediction_patterns'] = len({
        tuple(block['events'][target['event']]['selected']
              for target in targets for block in row['prediction']['blocks']
              if block['id'] == target['block']) for row in ledger['cells']})
    ledger['denominator']['distinct_actual_patterns'] = len({
        tuple(row['actual_target_selections']) for row in ledger['cells']})
    ledger['interpretation'] = (
        'Diagnostic source-order controls; target is baseline observed selections. '
        'Multiple matches do not identify original source. '
        + ('Selected blocks demonstrate different selection patterns across orders.'
           if ledger['denominator']['distinct_actual_patterns'] > 1 else
           'No differing selection pattern observed for these selected blocks; '
           'this family does not demonstrate noncommutation.'))
    require(digest(source) == ledger['inputs']['source_sha256'],
            'production source changed while pilot ran')
    require(all(digest(ROOT / path) == expected for path, expected in
                ledger['inputs']['tool_sha256'].items()),
            'pilot/capture/model/observer code changed while pilot ran')
    require(ledger['denominator']['compiled_cells'] == 6, 'incomplete six-cell compilation')
    require(not ledger['denominator']['invalid_model_cells'],
            'held-out compiler capture invalidated the calibrated model domain')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path,
                        help='fresh directory beneath this repository build/')
    args = parser.parse_args()
    output = args.output.resolve()
    try:
        output.relative_to(ROOT / 'build')
    except ValueError:
        parser.error('--output must be beneath this repository build/')
    if output.exists():
        parser.error('--output must not exist (invalid runs are preserved)')
    output.mkdir(parents=True)
    ledger = {'status': 'running', 'commands': [], 'cells': [],
              'denominator': {'declared_cells': 6, 'compiled_cells': 0,
                              'verified_cells': 0, 'invalid_model_cells': 0,
                              'body_verdicts': 0},
              'timing_note': ('model process CPU and wall are separate; forward capture '
                  'contains tool-level wall and child CPU records; Docker launcher CPU '
                  'does not measure guest compiler CPU; no speedup claim')}
    write_json(output / 'results.json', ledger)
    try:
        run_pilot(output, ledger)
        ledger['status'] = 'completed'
        code = 0
    except Exception as error:
        ledger['status'] = 'failed'
        ledger['error'] = '%s: %s' % (type(error).__name__, error)
        print(ledger['error'], file=sys.stderr)
        code = 1
    write_json(output / 'results.json', ledger)
    print('pilot %s: %s; results=%s' %
          (ledger['status'], json.dumps(ledger['denominator'], sort_keys=True),
           output / 'results.json'), flush=True)
    return code


if __name__ == '__main__':
    sys.exit(main())
