#!/usr/bin/env python3
"""Run installed cc1plus observations and require unchanged emitted objects."""
import hashlib
import argparse
import json
import shlex
import shutil
import subprocess
from pathlib import Path
import playbook_small_patterns as d
from gcc3_uref_stack_audit import validate_trace

ROOT = Path(__file__).resolve().parents[1]

def run(command, folder, label):
    with (folder/(label+'.log')).open('w') as log:
        subprocess.run(command, cwd=folder, stdout=log, stderr=subprocess.STDOUT, check=True)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reproduction-dir', type=Path, default=ROOT/'build/gcc3-uref-stack-reproduction')
    args = ap.parse_args()
    source = args.reproduction_dir.resolve()
    prior = json.loads((source/'results.json').read_text())
    image = prior['config'].splitlines()[0].split(' ', 1)[1]
    out = ROOT/'build/gcc3-uref-stack-observation'
    out.mkdir(exist_ok=True)
    compiler = out/'cc1plus'
    with compiler.open('wb') as stream:
        subprocess.run(['docker', 'run', '--rm', '--platform', 'linux/386', image, 'cat',
                        '/usr/libexec/gcc/i386-pc-linux-gnu/3.4.2/cc1plus'], stdout=stream, check=True)
    compiler.chmod(0o755)
    family = 'V90AutoDigitalImpDetector'
    result = {'revision': prior['revision'], 'config': prior['config'],
              'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(), 'cells': {}}
    for label, cell in prior['families'][family]['cells'].items():
        inputs = source/family/label
        folder = out/label
        folder.mkdir(exist_ok=True)
        shutil.copy2(inputs/(family+'.ii'), folder/(family+'.ii'))
        log = (inputs/'compile.log').read_text().splitlines()
        command = shlex.split(next(line for line in log if '/cc1plus -fpreprocessed' in line))
        assembler = shlex.split(next(line for line in log if '/bin/as ' in line))
        command[0] = str(compiler)
        plain = list(command)
        plain[plain.index('-o')+1] = 'plain.s'
        run(plain, folder, 'plain')
        observed = list(command)
        observed[observed.index('-o')+1] = 'observed.s'
        gdb = ['gdb', '-q', '-batch', '-ex', 'set pagination off', '-ex',
               'source '+str(ROOT/'tools/gcc3_uref_stack_gdb.py'), '--args']+observed
        run(gdb, folder, 'gdb')
        assert (inputs/(family+'.s')).read_bytes() == (folder/'plain.s').read_bytes() == (folder/'observed.s').read_bytes()
        virtual = '/work/'+label
        assembler[assembler.index('-o')+1] = virtual+'/observed.o'
        assembler[-1] = 'observed.s'
        assemble = d.tc.docker_prefix(image, ROOT, out, True)+['/bin/sh', '-c', 'cd '+shlex.quote(virtual)+' && '+shlex.join(assembler)]
        run(assemble, folder, 'assembler')
        assert (inputs/'candidate.o').read_bytes() == (folder/'observed.o').read_bytes()
        trace = json.loads((folder/'stack-observe.json').read_text())
        assert trace['exit_codes'] == [0] and not trace['errors'] and not trace['pending'] and trace['events'] and trace['frames'] and trace['callees']
        assert all('returned' in event and 'frame_after' in event for event in trace['events'])
        validate_trace(trace, label.startswith('constant-1'))
        result['cells'][label] = {'commands': [plain, gdb, assemble], 'object_sha256': cell['object_hash'],
                                  'raw_object_unchanged': True, 'trace': trace}
        (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
        (out/('results-'+source.name+'.json')).write_text(json.dumps(result, indent=2)+'\n')
        print(label, len(trace['events']), 'target allocations; raw output unchanged', flush=True)

if __name__ == '__main__':
    main()
