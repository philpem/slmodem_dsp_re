#!/usr/bin/env python3
"""Replay three hash-checked V22 controls through read-only boundary tracing."""
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import playbook_small_patterns as d
from gentoo_peep2_search_reproduce import PINS, IMAGE


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    out = d.ROOT / 'build/residual-v22-allocator'
    out.mkdir(exist_ok=True)
    compiler = out / 'cc1'
    archived = d.ROOT / 'build/residual-stage-scratch/cc1'
    if archived.exists():
        shutil.copyfile(archived, compiler)
    else:
        with compiler.open('wb') as stream:
            subprocess.run(['docker', 'run', '--rm', IMAGE, 'cat',
                            '/usr/libexec/gcc/i386-pc-linux-gnu/3.4.2/cc1'],
                           stdout=stream, check=True)
    compiler.chmod(0o755)
    assert sha(compiler) == PINS['cc1']
    prior = json.loads((d.ROOT / 'build/residual-v22-ack/results.json').read_text())
    assert prior['config'] == (d.ROOT / 'build/tc_out/.build-config').read_text()
    assert all(sha(d.ROOT / p) == h for p, h in prior['headers'].items())
    results = {'domain': 'docs/residual-v22-allocator-domain.md', 'compiler_hash': sha(compiler),
               'config': prior['config'], 'headers': prior['headers'],
               'source_revision': prior['revision'],
               'observer_hash': sha(d.ROOT / 'tools/residual_v22_allocator_gdb.py'), 'cells': []}
    for label, source_label in (('baseline', 'baseline'), ('combined', 'square-1-late-1'),
                                 ('baseline-repeat', 'baseline')):
        source = d.ROOT / 'build/residual-v22-ack/v22prc' / source_label
        saved = prior['families']['v22prc']['cells'][source_label]
        assert sha(source / 'candidate.o') == saved['object_hash']
        folder = out / label
        folder.mkdir(exist_ok=True)
        shutil.copyfile(source / 'v22prc.i', folder / 'v22prc.i')
        log = (source / 'compile.log').read_text()
        assert '-DDSPLIB_REPRODUCE_BUGS' in log
        cc1 = shlex.split(next(x for x in log.splitlines() if '/cc1 -fpreprocessed ' in x))
        cc1[0] = str(compiler)
        cc1[cc1.index('-fpreprocessed') + 1] = 'v22prc.i'
        cc1[cc1.index('-auxbase-strip') + 1] = 'candidate.o'
        cc1[cc1.index('-o') + 1] = 'plain.s'
        commands = []
        def run(command, name):
            commands.append(command)
            with (folder / (name + '.log')).open('w') as stream:
                subprocess.run(command, cwd=folder, stdout=stream, stderr=subprocess.STDOUT, check=True)
        run(cc1, 'plain')
        cc1[cc1.index('-o') + 1] = 'traced.s'
        run(['gdb', '-nx', '-q', '-batch', '-ex', 'set pagination off', '-ex',
             'source ' + str(d.ROOT / 'tools/residual_v22_allocator_gdb.py'), '--args'] + cc1, 'traced')
        trace_log = (folder / 'traced.log').read_text()
        assert not any(x in trace_log for x in ('Python Exception', 'Traceback (most recent call last)', 'Error occurred in Python'))
        # -dP prints compiler heap addresses in explicit tree annotations.
        # Preserve all operands/constants; raw assembled objects decide identity.
        def annotated(path):
            return re.sub(r'(<(?:[a-z_]+(?:_decl|_type)|string_cst) )0x[0-9a-f]+', r'\1<tree-address>', path.read_text())
        assert annotated(folder / 'plain.s') == annotated(folder / 'traced.s') == annotated(source / 'v22prc.s')
        run(['docker', 'run', '--rm', '--entrypoint', '/bin/sh', '-v', str(folder) + ':/trace', IMAGE,
             '-c', 'set -e; cd /trace; /usr/i386-pc-linux-gnu/bin/as -V -Qy -o plain.o plain.s; /usr/i386-pc-linux-gnu/bin/as -Qy -o traced.o traced.s'], 'assemble')
        assert sha(folder / 'plain.o') == sha(folder / 'traced.o') == saved['object_hash']
        observation = json.loads((folder / 'observe.json').read_text())
        assert not observation['errors'] and observation['exits'] == [0]
        results['cells'].append({'label': label, 'commands': commands,
                                'input_hash': sha(folder / 'v22prc.i'), 'object_hash': saved['object_hash'],
                                'events': observation['events'], 'raw_object_control': True})
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        print(label, '3 snapshots; saved/plain/traced objects equal', flush=True)
    assert results['cells'][0]['events'] == results['cells'][2]['events']
    assert results['cells'][0]['events'] == results['cells'][1]['events']
    assert all(sum(e['stage'] == 'reload-choice' for e in row['events']) == 15 for row in results['cells'])
    print('3 full-TU raw controls / 9 snapshots; independent baseline repeat equal')
    print('45 per-instruction reload choices; all three complete target traces equal')


if __name__ == '__main__':
    main()
