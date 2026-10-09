#!/usr/bin/env python3
"""Replay one saved compound SDM control to witness its actual movhi emitter."""
import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replay', type=Path, required=True,
                        help='completed gentoo_cc1_sdm_trace_reproduce output')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    replay, output = args.replay.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    for name in ('raw.s','traced.s','raw.o','traced.o'):
        (output/name).unlink(missing_ok=True)
    saved = json.loads((replay/'commands.json').read_text())
    commands = []
    def run(argv, label):
        result = subprocess.run(argv, cwd=output, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True)
        (output/(label+'.log')).write_text(result.stdout)
        commands.append({'command': argv, 'cwd': str(output), 'exit': result.returncode})
        (output/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        if result.returncode or 'Python Exception' in result.stdout or 'Error occurred in Python' in result.stdout:
            raise RuntimeError('invalid '+label)
        return result.stdout
    shutil.copyfile(replay/'compound-postincrement/SDM.i', output/'SDM.i')
    raw = next(row['command'] for row in saved if row['log']=='compound-postincrement-raw.log')
    run(raw, 'raw')
    trace = next(row['command'][:] for row in saved if row['log']=='compound-postincrement-trace.log')
    for index, argument in enumerate(trace):
        if argument.startswith('source '):
            trace[index] = 'source '+str(Path(__file__).with_name('gentoo_cc1_sdm_movhi_trace.py').resolve())
    logged = run(trace, 'trace')
    if logged.count('MOVHI UID118 pattern') != 1 or 'MOVHI attr type TYPE_IMOVX' not in logged:
        raise RuntimeError('unexpected emitter trace denominator or path')
    if 'exited normally' not in logged or 'dst_mode=HImode src_mode=HImode alternative=2 tune=PROCESSOR_PENTIUMPRO' not in logged:
        raise RuntimeError('unexpected RTL modes or tuning')
    if 'MOVHI emitted template movz{wl|x}' not in logged:
        raise RuntimeError('missing widening emitter template')
    image = 'ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest'
    run(['docker', 'run', '--rm', '--entrypoint', '/bin/sh', '-v', str(output)+':/trace', image,
         '-c', 'set -e; cd /trace; /usr/i386-pc-linux-gnu/bin/as -V -Qy -o raw.o raw.s; /usr/i386-pc-linux-gnu/bin/as -Qy -o traced.o traced.s'], 'assembler')
    paths = [output/'raw.o', output/'traced.o', replay/'compound-postincrement/raw.o']
    hashes = [hashlib.sha256(path.read_bytes()).hexdigest() for path in paths]
    if len(set(hashes)) != 1:
        raise RuntimeError('complete-object mismatch')
    (output/'results.json').write_text(json.dumps({'output_40_uid118_visits': 1,
        'complete_objects_equal': 3, 'object_sha256': hashes[0],
        'source_paths': [str(path) for path in paths], 'source_or_rtl_mutation': False}, indent=2)+'\n')
    print('1 emitter witness; 3/3 complete objects identical')


if __name__ == '__main__':
    main()
