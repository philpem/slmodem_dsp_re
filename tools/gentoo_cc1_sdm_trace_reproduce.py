#!/usr/bin/env python3
"""Replay saved full-TU SDM preprocessed controls with hash-pinned cc1/GDB."""
import argparse
import hashlib
import json
import shlex
import shutil
import subprocess
from pathlib import Path

IMAGE = 'ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest'
CC1 = '/usr/libexec/gcc/i386-pc-linux-gnu/3.4.2/cc1'
EXPECTED = '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    commands = []
    def run(command, label, cwd=None, env=None):
        result = subprocess.run(command, cwd=cwd, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (output / (label + '.log')).write_text(result.stdout)
        commands.append({'command': command, 'cwd': str(cwd) if cwd else None,
                         'environment_override': env, 'exit': result.returncode,
                         'log': label + '.log'})
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if result.returncode or 'Python Exception' in result.stdout or 'Error occurred in Python' in result.stdout:
            raise RuntimeError('invalid run: ' + label)
        return result.stdout
    run(['docker', 'run', '--rm', '--entrypoint', '/bin/sh', '-v', str(output)+':/trace',
         IMAGE, '-c', 'set -e; cp '+CC1+' /trace/cc1; file '+CC1+'; nm '+CC1+' | egrep "reload_cse_simplify|cselib_lookup"; readelf -S '+CC1+' | egrep "debug|symtab"'], 'installed-compiler')
    if digest(output/'cc1') != EXPECTED:
        raise RuntimeError('installed cc1 hash differs from traced address pin')
    run(['docker', 'image', 'inspect', IMAGE, '--format', '{{.Id}}'], 'image-id')
    run(['gdb', '--version'], 'debugger-version')
    run([str(output/'cc1'), '--version'], 'compiler-version')
    trace = Path(__file__).with_name('gentoo_cc1_sdm_trace.py').resolve()
    results = []
    for cell, uid in [('baseline', 56), ('mask-postincrement', 56), ('compound-postincrement', 57)]:
        source = args.controls.resolve()/'SDM'/cell
        destination = output/cell
        destination.mkdir(exist_ok=True)
        for name in ('raw.s','traced.s','raw.o','traced.o'):
            (destination/name).unlink(missing_ok=True)
        shutil.copyfile(source/'SDM.i', destination/'SDM.i')
        log = (source/'compile.log').read_text()
        line = next(line for line in log.splitlines() if '/cc1 -fpreprocessed ' in line)
        saved = shlex.split(line)
        flags = saved[1:]
        # Input is exactly the saved preprocessed full TU. Only apparatus paths change.
        flags[flags.index('-fpreprocessed')+1] = 'SDM.i'
        flags[flags.index('-auxbase-strip')+1] = 'candidate.o'
        flags[flags.index('-o')+1] = 'raw.s'
        run([str(output/'cc1')]+flags, cell+'-raw', destination)
        flags[flags.index('-o')+1] = 'traced.s'
        trace_output = run(['gdb', '-nx', '-batch', '-ex', 'file '+str(output/'cc1'),
             '-ex', 'set environment SDM_TRACE_UID '+str(uid),
             '-ex', 'python import os; os.environ["SDM_TRACE_UID"] = "'+str(uid)+'"',
             '-ex', 'source '+str(trace), '-ex', 'run '+shlex.join(flags)],
            cell+'-trace', destination)
        if trace_output.count('TRACE UID38 simplify_set input') != 2 or trace_output.count('TRACE UID%d simplify_set input' % uid) != 2:
            raise RuntimeError('unexpected trace denominator: '+cell)
        run(['docker', 'run', '--rm', '--entrypoint', '/bin/sh', '-v', str(destination)+':/trace',
             IMAGE, '-c', 'set -e; cd /trace; /usr/i386-pc-linux-gnu/bin/as -V -Qy -o raw.o raw.s; /usr/i386-pc-linux-gnu/bin/as -Qy -o traced.o traced.s'], cell+'-assembler')
        hashes = {label: digest(path) for label, path in
                  [('saved-object', source/'candidate.o'), ('raw-object', destination/'raw.o'),
                   ('traced-object', destination/'traced.o')]}
        if len(set(hashes.values())) != 1:
            raise RuntimeError('raw/debugged/container object mismatch: '+cell)
        results.append({'cell': cell, 'selected_uid': uid, 'object_hashes': hashes,
                        'input_sha256': digest(destination/'SDM.i'), 'gdb_helper_sha256': digest(trace), 'saved_cc1_command': saved,
                        'wide_read_simplify_visits': 2, 'selected_uid_simplify_visits': 2,
                        'path_map': {'saved_source': str(source), 'replay_directory': str(destination)},
                        'raw_trace_container_object_equal': True})
    (output/'results.json').write_text(json.dumps({'cc1_sha256': EXPECTED,
        'runtime': 'extracted unchanged image cc1 under host gdb and host 32-bit runtime; image-selected assembler',
        'controls': results, 'complete_objects_equal': len(results),
        'source_or_rtl_mutation': False}, indent=2)+'\n')
    print('%d/%d raw/debugged/container complete objects identical' % (len(results), len(results)))


if __name__ == '__main__':
    main()
