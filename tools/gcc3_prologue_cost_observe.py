#!/usr/bin/env python3
"""Observe prologue cost/cache decisions in the selected period cc1/cc1plus.

Inputs are a playbook_small_patterns reproduction made with -v -save-temps.
The compiler is extracted from its recorded image, never a host compiler.
The selected period assembler must reproduce each complete object byte for byte.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
_script_path = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
sys.path[:] = _script_path
import experiment_toolchain as tc

ROOT = Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reproduction-dir', required=True, type=Path)
    ap.add_argument('--family', required=True)
    ap.add_argument('--target', required=True, help='source or assembler name; use the mangled name for overloads')
    ap.add_argument('--output-dir', required=True, type=Path)
    ap.add_argument('--labels', nargs='+', help='default: all recorded cells')
    args = ap.parse_args()
    source, out = args.reproduction_dir.resolve(), args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    prior = json.loads((source/'results.json').read_text())
    cells = prior['families'][args.family]['cells']
    assert cells['baseline']['baseline_reproduced'], 'no raw full-TU baseline control'
    labels = args.labels or list(cells)
    assert labels and set(labels) <= set(cells), 'unknown or empty cell set'
    image = prior['config'].splitlines()[0].split(' ', 1)[1]
    report = {'revision': prior['revision'], 'config': prior['config'],
              'target': args.target, 'requested_cells': labels, 'cells': {}}
    for label in labels:
        assert tc.REPRODUCE_BUGS in shlex.split(cells[label]['command'][-1]), 'missing reproduction define'
        inputs, folder = source/args.family/label, out/label
        folder.mkdir(exist_ok=True)
        lines = (inputs/'compile.log').read_text().splitlines()
        command = shlex.split(next(s for s in lines if ' -fpreprocessed ' in s))
        assembler = shlex.split(next(s for s in lines if '/bin/as ' in s))
        compiler = out/Path(command[0]).name
        selected = subprocess.check_output(['docker', 'run', '--rm', '--platform',
                                            'linux/386', image, 'cat', command[0]])
        expected = hashlib.sha256(selected).hexdigest()
        if not compiler.exists():
            compiler.write_bytes(selected)
            compiler.chmod(0o755)
        # A reused output path cannot silently substitute another compiler.
        assert hashlib.sha256(compiler.read_bytes()).hexdigest() == expected
        preprocessed = Path(command[command.index('-fpreprocessed')+1]).name
        shutil.copy2(inputs/preprocessed, folder/preprocessed)
        command[0] = str(compiler)
        command[command.index('-o')+1] = 'observed.s'
        debugger = ['gdb', '-q', '-batch', '-ex', 'set pagination off', '-ex',
                    'source '+str(ROOT/'tools/gcc3_prologue_cost_gdb.py'), '--args']+command
        env = dict(os.environ, GCC3_PROLOGUE_TARGET=args.target)
        with (folder/'gdb.log').open('w') as log:
            subprocess.run(debugger, cwd=folder, env=env, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        virtual = '/work/'+label
        assembler[assembler.index('-o')+1] = virtual+'/observed.o'
        assembler[-1] = 'observed.s'
        assemble = tc.docker_prefix(image, ROOT, out, True)+[
            '/bin/sh', '-c', 'cd '+shlex.quote(virtual)+' && '+shlex.join(assembler)]
        with (folder/'as.log').open('w') as log:
            subprocess.run(assemble, stdout=log, stderr=subprocess.STDOUT, check=True)
        original = (inputs/'candidate.o').read_bytes()
        assert hashlib.sha256(original).hexdigest() == cells[label]['object_hash']
        assert original == (folder/'observed.o').read_bytes(), 'observer output drift'
        trace = json.loads((folder/'prologue-observe.json').read_text())
        assert trace['exits'] == [0] and not trace['errors'] and not trace['pending']
        assert trace['frames'] and trace['costs'], 'empty target observation'
        for cost in trace['costs']:
            weighted = 0
            for sample in cost['samples']:
                weighted += sample['frequency']
                assert sample['weighted_count'] == weighted, 'cost sample arithmetic drift'
                assert sample['threshold_weighted'] == cost['threshold']*cost['entry_frequency']
                assert sample['uid'] >= 0 and sample['rtx_code'] in ('INSN', 'JUMP_INSN', 'CALL_INSN')
        report['cells'][label] = {'compiler_sha256': expected, 'commands': [debugger, assemble],
                                  'raw_object_unchanged': True, 'trace': trace}
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
        print(label, len(trace['frames']), 'layouts;', len(trace['costs']),
              'cost decisions; raw object unchanged', flush=True)
    print(len(report['cells']), '/', len(labels), 'requested cells observed')


if __name__ == '__main__':
    main()
