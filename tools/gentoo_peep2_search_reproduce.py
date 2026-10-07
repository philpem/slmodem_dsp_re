#!/usr/bin/env python3
"""Replay whole-TU scratch-search controls without changing compiler state."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

# Avoid the local dis.py shadowing stdlib exception reporting on invalid runs.
_search = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
sys.path[:] = _search

ROOT = Path(__file__).resolve().parents[1]
IMAGE = 'ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest'
PINS = {
    'cc1': '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc',
    'cc1plus': 'b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d',
}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fax-controls', type=Path, default=ROOT/'build/fax-quality-use/V29r_int')
    parser.add_argument('--constructor-controls', type=Path, default=ROOT/'build/v90-parameters-ratchet-ab')
    parser.add_argument('--output', type=Path, default=ROOT/'build/gentoo-peep2-search-witnesses')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    commands = []
    def run(command, label, cwd=None, compiler_hash=None):
        env = dict(os.environ)
        if compiler_hash:
            env['PEEP2_COMPILER_SHA256'] = compiler_hash
        result = subprocess.run(command, cwd=cwd, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (output/(label+'.log')).write_text(result.stdout)
        commands.append({'command': command, 'cwd': str(cwd) if cwd else None,
                         'compiler_hash_override': compiler_hash, 'exit': result.returncode,
                         'log': label+'.log'})
        (output/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        if result.returncode or any(x in result.stdout for x in ('Python Exception', 'Error occurred in Python', 'Traceback (most recent call last)')):
            raise RuntimeError('invalid replay: '+label)
        return result.stdout
    shell = 'set -e; cp /usr/libexec/gcc/i386-pc-linux-gnu/3.4.2/cc1 /trace/cc1; cp /usr/libexec/gcc/i386-pc-linux-gnu/3.4.2/cc1plus /trace/cc1plus'
    run(['docker', 'run', '--rm', '--entrypoint', '/bin/sh', '-v', str(output)+':/trace', IMAGE, '-c', shell], 'extract-compiler')
    for compiler, expected in PINS.items():
        if sha(output/compiler) != expected:
            raise RuntimeError('unrecognized installed compiler: '+compiler)
        run([str(output/compiler), '--version'], compiler+'-version')
    run(['docker', 'image', 'inspect', IMAGE, '--format', '{{.Id}}'], 'image-id')
    run(['gdb', '--version'], 'gdb-version')
    controls = [
        ('fax-retained', args.fax_controls/'baseline', 'cc1', 'V29r_int.i'),
        ('fax-split-tail', args.fax_controls/'tail-1-member-0-narrow-0', 'cc1', 'V29r_int.i'),
        ('fax-retained-repeat', args.fax_controls/'baseline', 'cc1', 'V29r_int.i'),
        ('ctor-pre', args.constructor_controls/'pre-434-retype', 'cc1plus', 'V90Parameters.ii'),
        ('ctor-post', args.constructor_controls/'post-434-retype', 'cc1plus', 'V90Parameters.ii'),
        ('ctor-pre-repeat', args.constructor_controls/'pre-434-retype', 'cc1plus', 'V90Parameters.ii'),
    ]
    observer = ROOT/'tools/gentoo_peep2_search_trace.py'
    rows = []
    for label, source, compiler, input_name in controls:
        source = source.resolve()
        folder = output/label
        folder.mkdir(exist_ok=True)
        for name in ('raw.s', 'traced.s', 'raw.o', 'traced.o', 'events.json'):
            (folder/name).unlink(missing_ok=True)
        shutil.copyfile(source/input_name, folder/input_name)
        log = (source/'compile.log').read_text()
        assert '-DDSPLIB_REPRODUCE_BUGS' in log, 'saved preprocessing omitted bug define'
        saved = shlex.split(next(x for x in log.splitlines() if '/'+compiler+' -fpreprocessed ' in x))
        flags = saved[1:]
        flags[flags.index('-fpreprocessed')+1] = input_name
        flags[flags.index('-auxbase-strip')+1] = 'candidate.o'
        flags[flags.index('-o')+1] = 'raw.s'
        run([str(output/compiler)]+flags, label+'-raw', folder)
        flags[flags.index('-o')+1] = 'traced.s'
        traced = run(['gdb', '-nx', '-batch', '-ex', 'file '+str(output/compiler),
                      '-ex', 'source '+str(observer), '-ex', 'run '+shlex.join(flags)],
                     label+'-trace', folder, PINS[compiler])
        events = [json.loads(x[6:]) for x in traced.splitlines() if x.startswith('PEEP2 ')]
        assert events and events[0]['kind'] == 'observer'
        assert any(x['kind'] == 'enter' for x in events), 'observer did not fire'
        (folder/'events.json').write_text(json.dumps(events, indent=2)+'\n')
        run(['docker', 'run', '--rm', '--entrypoint', '/bin/sh', '-v', str(folder)+':/trace', IMAGE,
             '-c', 'set -e; cd /trace; /usr/i386-pc-linux-gnu/bin/as -V -Qy -o raw.o raw.s; /usr/i386-pc-linux-gnu/bin/as -Qy -o traced.o traced.s'], label+'-assemble')
        hashes = {name: sha(path) for name, path in [('saved', source/'candidate.o'), ('raw', folder/'raw.o'), ('traced', folder/'traced.o')]}
        assert len(set(hashes.values())) == 1, (label, hashes)
        rows.append({'control': label, 'compiler': compiler, 'compiler_sha256': PINS[compiler],
                     'saved_cc1_command': saved, 'preprocessed_sha256': sha(folder/input_name),
                     'observer_sha256': sha(observer), 'source_directory': str(source),
                     'complete_object_hashes': hashes,
                     'searches': sum(e['kind'] == 'enter' for e in events),
                     'candidate_visits': sum(e['kind'] == 'candidate' for e in events)})
        (output/'results.json').write_text(json.dumps({'runtime': 'unchanged image binaries under host GDB/32-bit runtime, image assembler',
            'source_or_rtl_mutation': False, 'controls': rows}, indent=2)+'\n')
        print(label, 'complete objects identical;', rows[-1]['searches'], 'searches;', rows[-1]['candidate_visits'], 'candidate visits', flush=True)

if __name__ == '__main__':
    main()
