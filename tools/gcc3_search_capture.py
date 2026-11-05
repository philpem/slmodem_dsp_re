#!/usr/bin/env python3
"""Capture an unchanged full TU and read-only GCC 3 scratch-search trace.

Artifacts are fresh and isolated. A valid capture requires repeated raw object
identity, native cc1 assembly identity, and observed assembly/object identity.
This validates the observation apparatus; the caller separately establishes
identity to its selected production baseline and interprets the complete TU.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import shlex
import subprocess
import sys
import time

import experiment_toolchain as tc

ROOT = Path(__file__).resolve().parents[1]
SUPPORTED = {
    'cc1': '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc',
    'cc1plus': 'b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d',
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def validate_trace(trace):
    require(not trace['errors'] and trace['exit_codes'] == [0] and trace['pending'] == 0,
            'incomplete compiler observation')
    events = trace['events']
    require(bool(events), 'empty scratch observation')
    for event in events:
        require(bool(event['function']) and 'selected' in event and
                'eligibility' in event and bool(event['candidates']), 'incomplete scratch event')
    for before, after in zip(events, events[1:]):
        require(before['cursor_after'] == after['cursor_before'], 'scratch cursor discontinuity')
    return events


def build_capture_flags(config, source, kind, rtl_dumps=False):
    flags = shlex.split(config['flags'])
    if kind == 'cc1plus':
        flags += shlex.split(config.get('cxx', ''))
    # DCR is a source-specific production override, selected by its source name.
    if Path(source).name == 'dcr.c':
        flags += shlex.split(config.get('dcr', ''))
    flags += ['-v', '-save-temps']
    if rtl_dumps:
        flags.append('-da')
    return flags


class Capture:
    def __init__(self, args):
        self.root = args.root.resolve()
        self.source = args.source.resolve()
        self.out = args.output.resolve()
        config = args.config or self.root / 'build/tc_out/.build-config'
        self.config = config.resolve()
        allowed = (self.root / 'build', Path('/mnt/zfs/dev/slmodem/scratch'))
        require(any(self.out.is_relative_to(p.resolve()) for p in allowed),
                'output must be under repository build or /mnt/zfs/dev/slmodem/scratch')
        require(not self.out.exists(), 'output directory already exists: ' + str(self.out))
        require(self.source.is_file() and self.source.suffix in ('.c', '.cpp'),
                'source must be an existing complete .c or .cpp translation unit')
        require(not self.source.is_relative_to(self.out), 'source cannot be inside output')
        self.out.mkdir(parents=True)
        self.result = {'schema': 1, 'status': 'incomplete', 'source': str(self.source),
                       'root': str(self.root), 'config_path': str(self.config), 'commands': [],
                       'rtl_dumps': bool(args.rtl_dumps),
                       'capture_sha256': sha(Path(__file__).resolve()),
                       'observer_sha256': sha(Path(__file__).with_name('gcc3_search_capture_gdb.py')),
                       'checks': {}, 'timing_scope': 'child CPU is host process and descendants; '
                       'container compiler CPU is separately reported by shell time'}
        self.save()

    def save(self):
        (self.out / 'results.json').write_text(json.dumps(self.result, indent=2) + '\n')

    def run(self, label, command, cwd=None, binary=None):
        cwd = cwd or self.out
        log = self.out / (label + '.log')
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        started = time.monotonic()
        with log.open('wb') as stream:
            if binary:
                with binary.open('wb') as output:
                    completed = subprocess.run(command, cwd=cwd, stdout=output, stderr=stream)
            else:
                completed = subprocess.run(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT)
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        record = {'label': label, 'argv': list(command), 'cwd': str(cwd),
                  'returncode': completed.returncode, 'wall_seconds': time.monotonic() - started,
                  'child_user_seconds': after.ru_utime - before.ru_utime,
                  'child_system_seconds': after.ru_stime - before.ru_stime,
                  'log': log.name}
        cpu = re.findall(r'^capture_cpu ([0-9.]+) ([0-9.]+) ([0-9.]+)$',
                         log.read_text(errors='replace'), re.M)
        if cpu:
            record['compiler_elapsed_seconds'], record['compiler_user_seconds'], record['compiler_system_seconds'] = map(float, cpu[-1])
        if binary:
            record['binary_output'] = str(binary.relative_to(self.out))
        self.result['commands'].append(record)
        self.save()
        require(completed.returncode == 0, '%s failed; inspect %s' % (label, log))
        return log.read_text(errors='replace')

    def docker(self, work=None):
        command = tc.docker_prefix(self.image, self.root, work or self.out, True)
        command[command.index(str(self.root) + ':/src')] += ':ro'
        command[command.index('-w') + 1] = '/work'
        command[2:2] = ['-v', str(self.source.parent) + ':/input:ro']
        return command

    def work(self, name):
        directory = self.out / name
        directory.mkdir()
        # Preserve configurable relative include/force-include spellings exactly.
        # The compiler runs in an artifact directory and sees the same headers
        # through read-only repository mounts, with no added compiler flags.
        for item in ('include', 'src', 'tools'):
            (directory / item).symlink_to('/src/' + item, target_is_directory=True)
        return directory

    def compile(self, name, source_path, flags):
        work = self.work(name)
        shell = tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags, 'candidate.o', source_path)
        if self.source.suffix == '.cpp':
            shell = shell.replace('exec gcc -c ', 'exec g++ -c ', 1)
        timed = ("TIMEFORMAT='capture_cpu %R %U %S'; time /bin/sh -c " +
                 shlex.quote(shell) + '; capture_rc=$?; exit "$capture_rc"')
        self.run(name + '-compile', self.docker(work) + ['/bin/bash', '-c', timed])
        return work

    def execute(self):
        config_text = self.config.read_text()
        config = {}
        for line in config_text.splitlines():
            if line.strip():
                key, value = line.split(None, 1)
                require(key not in config, 'duplicate config key: ' + key)
                config[key] = value
        require('image' in config and 'flags' in config, 'config needs image and flags')
        identity = self.run('image-inspect', ['docker', 'image', 'inspect', config['image']])
        image_info = json.loads(identity)
        require(len(image_info) == 1, 'image did not resolve uniquely')
        self.image = image_info[0]['Id']
        require(bool(re.fullmatch(r'sha256:[0-9a-f]{64}', self.image)), 'invalid immutable image ID')
        self.result.update({'config': config_text, 'config_sha256': sha(self.config),
                            'image_requested': config['image'], 'image_id': self.image,
                            'image_inspect': image_info, 'source_sha256': sha(self.source),
                            'compiler_path': tc.GENTOO_COMPILER_PATH})
        (self.out / '.build-config').write_text(config_text)
        # Retain the exact full input without compiling a truncated extraction.
        (self.out / ('source-input' + self.source.suffix)).write_bytes(self.source.read_bytes())
        header_paths = self.run('tracked-headers', ['git', '-C', str(self.root), 'ls-files', '-z',
                                '--', 'include', 'src', 'tools/toolchain/period_compat.h']).split('\0')
        headers = [p for p in header_paths if p and
                   (Path(p).suffix in ('.h', '.hpp', '.hh', '.hxx') or p.startswith('include/'))]
        self.result['header_sha256'] = {p: sha(self.root / p) for p in headers}
        self.result['revision'] = self.run('revision', ['git', '-C', str(self.root), 'rev-parse', 'HEAD']).strip()
        # Also record local quoted-header candidates for sources outside src/.
        self.result['source_parent_header_sha256'] = {
            str(p): sha(p) for p in sorted(self.source.parent.iterdir())
            if p.is_file() and p.suffix in ('.h', '.hpp', '.hh', '.hxx')}
        version_shell = ('export PATH=' + shlex.quote(tc.GENTOO_COMPILER_PATH) + ':$PATH; '
                         'gcc --version; g++ --version; gcc -dumpmachine; '
                         'replay_as_path=$(gcc -print-prog-name=as); '
                         'printf "selected assembler: %s\\n" "$replay_as_path"; '
                         '"$replay_as_path" --version')
        self.run('toolchain-identity', self.docker() + ['/bin/sh', '-c', version_shell])
        self.run('gdb-identity', ['gdb', '--version'])
        self.run('docker-identity', ['docker', '--version'])
        kind = 'cc1plus' if self.source.suffix == '.cpp' else 'cc1'
        executable = self.out / kind
        self.run('extract-' + kind, ['docker', 'run', '--rm', '--platform', 'linux/386',
                                  self.image, 'cat', '/usr/libexec/gcc/i386-pc-linux-gnu/3.4.2/' + kind],
                 binary=executable)
        executable.chmod(0o755)
        self.result['compiler_kind'] = kind
        self.result['compiler_sha256'] = sha(executable)
        require(sha(executable) == SUPPORTED[kind], 'unsupported compiler executable hash; hook not calibrated')
        self.result['hook'] = {'source': '/build/gcc-3.4.2/gcc/recog.c:2984',
                               'validation': 'pinned calibrated executable SHA-256'}
        flags = build_capture_flags(config, self.source, kind, self.result['rtl_dumps'])
        self.result['effective_flags'] = tc.add_reproduce_bugs(flags)
        source_path = ('/src/' + self.source.relative_to(self.root).as_posix()
                       if self.source.is_relative_to(self.root) else '/input/' + self.source.name)
        first = self.compile('initial', source_path, flags)
        repeat = self.compile('repeat', source_path, flags)
        obj = first / 'candidate.o'
        require(obj.read_bytes() == (repeat / 'candidate.o').read_bytes(), 'repeated full-TU object differs')
        self.result['checks']['repeated_object_equal'] = True
        log = (self.out / 'initial-compile.log').read_text()
        commands = [shlex.split(line.strip()) for line in log.splitlines() if line.strip().startswith('/')]
        cc_commands = [c for c in commands if Path(c[0]).name == kind and '-fpreprocessed' in c]
        as_commands = [c for c in commands if Path(c[0]).name == 'as' and '-o' in c]
        require(len(cc_commands) == len(as_commands) == 1, 'cannot identify unique cc1/assembler commands')
        original_cc, assembler = cc_commands[0], as_commands[0]
        self.result['original_cc_command'] = original_cc
        self.result['original_assembler_command'] = assembler
        preprocessed = list(first.glob('*.ii' if kind == 'cc1plus' else '*.i'))
        assembly = list(first.glob('*.s'))
        require(len(preprocessed) == len(assembly) == 1, 'missing/ambiguous full-TU saved input/assembly')
        preprocessed = preprocessed[0]
        local_input = self.out / preprocessed.name
        local_input.write_bytes(preprocessed.read_bytes())
        native = [str(executable)] + original_cc[1:]
        # GCC's verbose command uses this saved input by basename. Accept only
        # the one captured input, rather than silently replaying another file.
        input_indices = [i for i, token in enumerate(native) if token == preprocessed.name or
                         token == '/work/' + preprocessed.name]
        require(len(input_indices) == 1, 'cannot identify preprocessed input operand')
        native[input_indices[0]] = preprocessed.name
        require('-o' in native, 'cc1 command lacks output')
        native[native.index('-o') + 1] = 'plain.s'
        self.run('native-plain', native)
        require((self.out / 'plain.s').read_bytes() == assembly[0].read_bytes(), 'native replay changed assembly')
        nm = self.run('compiler-symbols', ['nm', str(executable)])
        symbols = {parts[2]: int(parts[0], 16) for line in nm.splitlines()
                   if len(parts := line.split()) == 3 and re.fullmatch('[0-9a-fA-F]+', parts[0])}
        require('search_ofs.0' in symbols, 'compiler lacks scratch cursor symbol')
        (self.out / 'observer-settings.json').write_text(json.dumps({'cursor_address': symbols['search_ofs.0']}) + '\n')
        observed = list(native)
        observed[observed.index('-o') + 1] = 'observed.s'
        observer = Path(__file__).with_name('gcc3_search_capture_gdb.py')
        require(sha(observer) == self.result['observer_sha256'], 'observer changed before observation')
        gdb = ['gdb', '-q', '-batch', '-ex', 'set pagination off', '-ex', 'set confirm off',
               '-ex', 'set auto-load off', '-ex', 'set may-call-functions off',
               '-ex', 'source ' + str(observer), '--args'] + observed
        self.run('native-observed', gdb)
        trace = json.loads((self.out / 'scratch-observe.json').read_text())
        events = validate_trace(trace)
        trace['provenance'] = {key: self.result[key] for key in
                               ('compiler_sha256', 'image_id', 'effective_flags', 'observer_sha256', 'capture_sha256')}
        (self.out / 'scratch-observe.json').write_text(json.dumps(trace, indent=2) + '\n')
        require((self.out / 'plain.s').read_bytes() == (self.out / 'observed.s').read_bytes(),
                'observation changed assembly')
        self.result['checks']['initial_plain_observed_assembly_equal'] = True
        assemble = list(assembler)
        assemble[assemble.index('-o') + 1] = 'observed.o'
        require(assemble[-1] == assembly[0].name, 'unexpected assembler input operand')
        assemble[-1] = 'observed.s'
        self.run('assemble-observed', self.docker() + assemble)
        require(obj.read_bytes() == (self.out / 'observed.o').read_bytes(), 'observation changed whole object')
        self.result['checks']['observed_object_equal'] = True
        require(sha(self.source) == self.result['source_sha256'], 'source changed during capture')
        require(all(sha(self.root / p) == digest for p, digest in self.result['header_sha256'].items()),
                'repository header changed during capture')
        require(all(sha(Path(p)) == digest for p, digest in self.result['source_parent_header_sha256'].items()),
                'source-parent header changed during capture')
        require(sha(self.config) == self.result['config_sha256'], 'configuration changed during capture')
        require(sha(Path(__file__).resolve()) == self.result['capture_sha256'],
                'capture apparatus changed during capture')
        require(sha(observer) == self.result['observer_sha256'], 'observer changed during capture')
        self.result.update({'status': 'valid', 'object_sha256': sha(obj),
                            'object': 'initial/candidate.o', 'trace': 'scratch-observe.json',
                            'preprocessed_sha256': sha(preprocessed), 'assembly_sha256': sha(assembly[0]),
                            'events': len(events), 'functions_with_events': len({e['function'] for e in events})})
        self.save()
        print(json.dumps({'status': 'valid', 'output': str(self.out), 'events': len(events),
                          'functions_with_events': self.result['functions_with_events'],
                          'object_sha256': self.result['object_sha256']}, sort_keys=True))


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='complete .c/.cpp TU')
    parser.add_argument('--output', type=Path, required=True, help='fresh artifact directory')
    parser.add_argument('--config', type=Path, help='default: ROOT/build/tc_out/.build-config')
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--rtl-dumps', action='store_true',
                        help='add GCC -da RTL dumps (opt in; some compiler paths can fail)')
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    capture = None
    try:
        capture = Capture(args)
        capture.execute()
    except (OSError, ValueError, RuntimeError, KeyError) as exc:
        if capture:
            capture.result.update({'status': 'invalid', 'error': str(exc)})
            capture.save()
        print('capture failed: ' + str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
