#!/usr/bin/env python3
"""Replay an immutable GCC3 capture with bounded read-only local allocation tracing."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import sys
import time

SUPPORTED = {
    'cc1': '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc',
    'cc1plus': 'b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d'}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_trace(trace):
    require(not trace['errors'] and trace['exit_codes'] == [0] and trace['pending'] == 0,
            'invalid trace: ' + str(trace['errors']))
    choices = [row for row in trace['events'] if row['kind'] == 'local_choice']
    passes = [row for row in trace['events'] if row['kind'] == 'pass_entry']
    counts = trace['denominators']
    require(len(choices) == counts['target_local_choices'] > 0, 'empty/inconsistent target choices')
    require(len(passes) == counts['target_pass_entries'] >= 2, 'missing target pass entries')
    require({'local_alloc', 'global_alloc'} <= {row['pass'] for row in passes}, 'missing allocation passes')
    require(counts['whole_tu_local_choices'] >= len(choices) and
            counts['whole_tu_pass_entries'] >= len(passes), 'invalid whole-TU denominator')
    require([row['sequence'] for row in trace['events']] == list(range(len(trace['events']))),
            'invalid event chronology')
    returns = [row for row in trace['events'] if row['kind'] == 'local_choice_return']
    require(len(returns) == len(choices), 'incomplete choice returns')
    for choice in choices:
        matches = [row for row in returns if row['choice_sequence'] == choice['sequence']]
        require(len(matches) == 1 and matches[0]['sequence'] > choice['sequence'] and
                matches[0]['selected'] == choice['return'] and choice.get('exclusions'),
                'incomplete choice/exclusion/return')
    return counts


def execute(args):
    capture, out = args.capture.resolve(), args.output.resolve()
    require(not out.exists(), 'output must be fresh')
    require(not out.is_relative_to(capture), 'output must be outside immutable capture')
    out.mkdir(parents=True)
    observer = Path(__file__).with_name('gcc3_alloc_trace_gdb.py').resolve()
    result = {'status': 'incomplete', 'capture': str(capture), 'function': args.function,
              'commands': [], 'checks': {}, 'tool_sha256': sha(Path(__file__).resolve()),
              'observer_sha256': sha(observer), 'capture_manifest_sha256': sha(capture / 'results.json')}
    def save():
        (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    def run(label, command):
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        started = time.monotonic()
        with (out / (label + '.log')).open('wb') as log:
            process = subprocess.run(command, cwd=out, stdout=log, stderr=subprocess.STDOUT)
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        result['commands'].append({'label': label, 'argv': command, 'returncode': process.returncode,
            'wall_seconds': time.monotonic() - started,
            'host_child_user_seconds': after.ru_utime - before.ru_utime,
            'host_child_system_seconds': after.ru_stime - before.ru_stime})
        save()
        return process.returncode
    save()
    try:
        original = json.loads((capture / 'results.json').read_text())
        require(original['status'] == 'valid' and all(original['checks'].get(key) is True for key in
            ('repeated_object_equal', 'initial_plain_observed_assembly_equal', 'observed_object_equal')), 'invalid capture')
        kind = original['compiler_kind']
        require(kind in SUPPORTED and original['compiler_sha256'] == SUPPORTED[kind], 'unsupported compiler')
        compiler = capture / kind
        require(sha(compiler) == original['compiler_sha256'], 'compiler changed')
        candidates = list((capture / 'initial').glob('*.ii' if kind == 'cc1plus' else '*.i'))
        require(len(candidates) == 1, 'ambiguous preprocessed input')
        source = candidates[0]
        require(sha(source) == original['preprocessed_sha256'], 'saved input changed')
        reference = capture / original['object']
        require(sha(reference) == original['object_sha256'], 'reference object changed')
        assemblies = list((capture / 'initial').glob('*.s'))
        require(len(assemblies) == 1 and sha(assemblies[0]) == original['assembly_sha256'], 'reference assembly changed')
        result['inputs'] = {'compiler_sha256': sha(compiler), 'preprocessed_sha256': sha(source),
                            'reference_object_sha256': sha(reference), 'reference_assembly_sha256': sha(assemblies[0]),
                            'image_id': original['image_id'], 'compiler_kind': kind}
        (out / source.name).write_bytes(source.read_bytes())
        native = [str(compiler)] + original['original_cc_command'][1:]
        indices = [i for i, token in enumerate(native) if token in (source.name, '/work/' + source.name)]
        require(len(indices) == 1 and '-o' in native, 'unsupported compiler command input/output')
        native[indices[0]] = source.name
        native[native.index('-o') + 1] = 'plain.s'
        require(run('native-plain', native) == 0, 'plain compilation failed')
        require(sha(out / 'plain.s') == original['assembly_sha256'], 'plain assembly changed')
        (out / 'alloc-settings.json').write_text(json.dumps({'function': args.function,
            'compiler_sha256': original['compiler_sha256']}) + '\n')
        observed = list(native)
        observed[observed.index('-o') + 1] = 'observed.s'
        command = ['gdb', '-q', '-batch', '-ex', 'set pagination off', '-ex', 'set confirm off',
            '-ex', 'set auto-load off', '-ex', 'set may-call-functions off',
            '-ex', 'source ' + str(observer), '--args'] + observed
        observer_rc = run('native-observed', command)
        # Even a refused/empty trace must prove observation left code unchanged.
        result['checks']['assembly_equal'] = (out / 'observed.s').is_file() and sha(out / 'observed.s') == original['assembly_sha256']
        require(result['checks']['assembly_equal'], 'observed assembly changed or absent')
        assembler = list(original['original_assembler_command'])
        require('-o' in assembler and assembler[-1] == assemblies[0].name, 'unsupported assembler command')
        assembler[assembler.index('-o') + 1] = 'observed.o'
        assembler[-1] = 'observed.s'
        command = ['docker', 'run', '--rm', '--platform', 'linux/386', '-v', str(out) + ':/work',
                   '-w', '/work', original['image_id']] + assembler
        require(run('assemble-observed', command) == 0, 'assembling observation failed')
        result['checks']['object_equal'] = sha(out / 'observed.o') == original['object_sha256']
        require(result['checks']['object_equal'], 'observed object changed')
        trace = json.loads((out / 'allocation-trace.json').read_text())
        result['denominators'] = trace['denominators']
        require(observer_rc == 0, 'observer rejected trace: ' + str(trace['errors']))
        validate_trace(trace)
        require(sha(compiler) == original['compiler_sha256'] and sha(source) == original['preprocessed_sha256']
                and sha(reference) == original['object_sha256'], 'capture changed during observation')
        require(sha(observer) == result['observer_sha256'] and
                sha(Path(__file__).resolve()) == result['tool_sha256'], 'trace apparatus changed')
        require(sha(capture / 'results.json') == result['capture_manifest_sha256'], 'capture manifest changed')
        result['status'] = 'valid'
        save()
        print(json.dumps({'status': 'valid', 'denominators': trace['denominators'], 'checks': result['checks']}))
        return 0
    except (OSError, ValueError, KeyError, RuntimeError) as exc:
        result.update(status='invalid', error=str(exc))
        save()
        print('allocation trace failed: ' + str(exc), file=sys.stderr)
        return 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--function', required=True, help='exact assembler function name')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        return execute(args)
    except (OSError, RuntimeError) as exc:
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
