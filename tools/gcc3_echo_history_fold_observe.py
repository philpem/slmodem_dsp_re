#!/usr/bin/env python3
"""Trace parser-time history allocation folding in installed Gentoo cc1plus.

Compiles unchanged complete source through the shared period helper; observes
the selected compiler without inferior calls/state edits; reassembles both
target and absent-target output with its selected period assembler. Require
full-object equality, actual assignment binding and a failing absent control.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import playbook_small_patterns as d

ROOT = Path(__file__).resolve().parents[1]
REV = 'c7cf4f8cd'
COMPILER = 'b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d'
SOURCE = 'src/pump/v90/V92EchoCanceller.cpp'
DUMPS = ('-v', '-save-temps', '-da', '-fdump-tree-original', '-fdump-tree-inlined')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def expression(node):
    if node is None:
        return 'NULL'
    code, operands = node['code'], node.get('operands', [])
    if 'name' in node:
        return node['name'] or code
    if code == 'INTEGER_CST':
        return str(node['low'])
    if code in ('NOP_EXPR', 'CONVERT_EXPR', 'NON_LVALUE_EXPR'):
        return expression(operands[0])
    symbols = {'PLUS_EXPR': '+', 'MINUS_EXPR': '-', 'MULT_EXPR': '*', 'TRUNC_DIV_EXPR': '/'}
    if code in symbols:
        return '(' + expression(operands[0]) + symbols[code] + expression(operands[1]) + ')'
    return code + '(' + ','.join(expression(operand) for operand in operands) + ')'


def validate(trace):
    assert trace['exit_codes'] == [0] and not trace['errors'] and trace['pending'] == 0
    assert len(trace['assignment_bindings']) == 1
    assignment = trace['assignment_bindings'][0]
    assert assignment['left']['operands'][1]['name'] == 'historyAlloc'
    assert expression(assignment['right']) == '(((((span/blockLen)+2)*blockLen)+span)+extra)'
    found = [(i, event) for i, event in enumerate(trace['events']) if event['hook'] == 'fold' and
             expression(event['input']) == '((blockLen*2)+((span/blockLen)*blockLen))' and
             expression(event['returned']) == '(((span/blockLen)+2)*blockLen)']
    assert len(found) == 1
    index, event = found[0]
    assert 'build_binary_op' in event['callers'] and 'cp_parser_additive_expression' in event['callers']
    assert event['line'] == trace['line_window'][1]
    def walk(node):
        yield node
        for child in node.get('operands', []):
            yield from walk(child)
    assert event['returned']['pointer'] in {node['pointer'] for node in walk(assignment['right'])}
    final_roots = [row for row in trace['events'] if row['hook'] == 'build_binary_op' and
                   row.get('returned', {}).get('pointer') == assignment['right']['pointer']]
    assert len(final_roots) == 1
    assert final_roots[0]['code'] == 'PLUS_EXPR'
    assert expression(final_roots[0]['operands'][1]) == 'extra'
    for node in walk(event['input']):
        if node['code'] in ('PARM_DECL', 'VAR_DECL', 'PLUS_EXPR', 'MULT_EXPR', 'TRUNC_DIV_EXPR'):
            assert node['type'] == {'precision': 32, 'unsigned': 1}
            assert node['side_effects'] == node['volatile'] == 0
    return index


def run(command, directory, label, env=None):
    with (directory / (label + '.log')).open('w') as log:
        return subprocess.run(command, cwd=directory, stdout=log, stderr=subprocess.STDOUT, env=env).returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir', required=True, type=Path)
    parser.add_argument('--reproduction-dir', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/echo-history-fold')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    frozen = args.baseline_dir.resolve()
    manifest = json.loads((frozen / 'manifest.json').read_text())
    assert manifest['revision'].startswith(REV) and len(manifest['objects']) == 300
    assert manifest['config'] == (frozen / '.build-config').read_text()
    if args.reproduction_dir:
        reproduction = args.reproduction_dir.resolve()
    else:
        d.REV, d.OUT_NAME, d.SOURCE_PATHS = REV, out.name + '-reproduction', (SOURCE,)
        d.DUMP_FLAGS = DUMPS
        d.variants = lambda path, source: {'baseline': source}
        sys.argv = [sys.argv[0], '--domain', 'unchanged-history-expression-fold-observation',
                    '--baseline-dir', str(frozen)]
        d.main()
        reproduction = ROOT / 'build' / d.OUT_NAME
    prior = json.loads((reproduction / 'results.json').read_text())
    assert prior['revision'] == REV and prior['config'] == manifest['config']
    assert all(sha(ROOT / path) == digest for path, digest in prior['headers'].items())
    cell = prior['families']['V92EchoCanceller']['cells']['baseline']
    inputs = reproduction / 'V92EchoCanceller/baseline'
    assert cell['baseline_reproduced'] and sha(inputs / 'candidate.o') == manifest['objects'][SOURCE.replace('/', '_') + '.o']
    assert sha(inputs / 'candidate.o') == cell['object_hash']
    source = subprocess.check_output(['git', 'show', REV + ':' + SOURCE], cwd=ROOT, text=True)
    assert (inputs / 'V92EchoCanceller.cpp').read_text() == source
    first_line = next(i for i, line in enumerate(source.splitlines(), 1) if 'historyAlloc = span +' in line)
    wrapper = shlex.split(cell['command'][-1])
    arguments = wrapper[wrapper.index('g++') + 1:]
    config_lines = prior['config'].splitlines()
    flags = shlex.split(next(line[6:] for line in config_lines if line.startswith('flags ')))
    flags = ['-I/src/include' if word == '-Iinclude' else '/src/' + word
             if word == 'tools/toolchain/period_compat.h' else word for word in flags]
    flags += list(DUMPS)
    flags += shlex.split(next(line[6:] for line in config_lines if line.startswith('cxx   ')))
    virtual_source = '/work/V92EchoCanceller/baseline'
    assert arguments == ['-c'] + flags + [d.tc.REPRODUCE_BUGS, '-o', virtual_source + '/candidate.o',
                                         virtual_source + '/V92EchoCanceller.cpp']
    image = prior['config'].splitlines()[0].split(' ', 1)[1]
    collect = ('set -e; export PATH=' + shlex.quote(d.tc.GENTOO_COMPILER_PATH) + ':$PATH; '
               'history_cc1=$(g++ -print-prog-name=cc1plus); printf "%s\\n" "$history_cc1"; '
               '"$history_cc1" --version')
    identity_command = d.tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', collect]
    assert run(identity_command, out, 'compiler-identity') == 0
    copy_script = ('set -e; export PATH=' + shlex.quote(d.tc.GENTOO_COMPILER_PATH) + ':$PATH; '
                   'history_cc1=$(g++ -print-prog-name=cc1plus); exec cat "$history_cc1"')
    copy_command = d.tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', copy_script]
    compiler = out / 'cc1plus'
    # Docker must not create a root-owned executable that the caller cannot
    # chmod on a clean replay. Host stdout owns the copied artifact. Remove
    # only this generated output if an older runner left it root-owned.
    compiler.unlink(missing_ok=True)
    with compiler.open('wb') as stream, (out / 'compiler-copy.log').open('w') as log:
        subprocess.run(copy_command, cwd=out, stdout=stream, stderr=log, check=True)
    compiler.chmod(0o755)
    assert sha(compiler) == COMPILER
    log = (inputs / 'compile.log').read_text().splitlines()
    assert 'GNU C++ version 3.4.2  (Gentoo Linux 3.4.2-r2' in '\n'.join(log)
    assert 'GNU assembler version 2.15.92.0.2 (i386-pc-linux-gnu)' in '\n'.join(log)
    frontend = shlex.split(next(line for line in log if '/cc1plus -fpreprocessed' in line))
    assembler = shlex.split(next(line for line in log if '/bin/as ' in line))
    frontend[0] = str(compiler)
    reports = {}
    for label, target in [('target', 'historyAlloc'), ('absent-target', 'NO_SUCH_HISTORY_FIELD')]:
        directory = out / label
        directory.mkdir(exist_ok=True)
        shutil.copy2(inputs / 'V92EchoCanceller.ii', directory / 'V92EchoCanceller.ii')
        plain = list(frontend)
        plain[plain.index('-o') + 1] = 'plain.s'
        assert run(plain, directory, 'plain') == 0
        observed = list(frontend)
        observed[observed.index('-o') + 1] = 'observed.s'
        debugger = ['gdb', '-q', '-batch', '-ex', 'set pagination off', '-ex',
                    'source ' + str(ROOT / 'tools/gcc3_echo_history_fold_gdb.py'), '--args'] + observed
        status = run(debugger, directory, 'gdb', dict(os.environ, GCC3_HISTORY_TARGET=target,
                     GCC3_HISTORY_FIRST_LINE=str(first_line)))
        trace = json.loads((directory / 'fold-observe.json').read_text())
        assert trace['exit_codes'] == [0] and not trace['errors'] and trace['pending'] == 0
        assert (status == 0) if label == 'target' else (status != 0)
        assert (directory / 'plain.s').read_bytes() == (directory / 'observed.s').read_bytes() == (inputs / 'V92EchoCanceller.s').read_bytes()
        if label == 'target':
            fold_index = validate(trace)
        else:
            assert not trace['events'] and not trace['assignment_bindings']
            assert trace['parser_candidate_events'] > 0
        assemble = list(assembler)
        virtual = '/work/' + label
        assemble[assemble.index('-o') + 1] = virtual + '/observed.o'
        assemble[-1] = 'observed.s'
        assemble_command = d.tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
                            'cd ' + shlex.quote(virtual) + ' || exit 1; exec ' + shlex.join(assemble)]
        assert run(assemble_command, directory, 'assembler') == 0
        assert (directory / 'observed.o').read_bytes() == (inputs / 'candidate.o').read_bytes()
        reports[label] = dict(commands=[plain, debugger, assemble_command], observer_exit=status,
            compiler_exit=0, raw_object_unchanged=True, object_sha256=sha(directory / 'observed.o'), trace=trace)
    wrong = copy.deepcopy(reports['target']['trace'])
    wrong['assignment_bindings'] = []
    try:
        validate(wrong)
    except AssertionError:
        pass
    else:
        raise AssertionError('missing assignment binding control accepted')
    wrong = copy.deepcopy(reports['target']['trace'])
    wrong['events'][fold_index]['returned'] = wrong['events'][fold_index]['input']
    try:
        validate(wrong)
    except AssertionError:
        pass
    else:
        raise AssertionError('missing factoring control accepted')
    wrong = copy.deepcopy(reports['target']['trace'])
    wrong['events'][fold_index]['input']['type']['unsigned'] = 0
    try:
        validate(wrong)
    except AssertionError:
        pass
    else:
        raise AssertionError('changed arithmetic type control accepted')
    report = dict(revision=manifest['revision'], config=prior['config'], compiler_sha256=COMPILER,
        compiler_identity_command=identity_command, compiler_copy_command=copy_command,
        baseline_manifest_sha256=sha(frozen / 'manifest.json'),
        actual_source_command=cell['command'], complete_source_arguments_checked=True,
        reproduction=str(reproduction), preprocessed_sha256=sha(inputs / 'V92EchoCanceller.ii'),
        observations=reports, first_common_factor_event=fold_index,
        controls=dict(absent_assignment_rejected_with_compiler_success=True,
                      missing_assignment_binding_rejected=True, missing_factoring_rejected=True,
                      changed_unsigned_type_rejected=True,
                      both_complete_objects_raw_equal=True), production_source_changed=False)
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Parser common-factor fold identified; actual assignment bound; 4 negatives; both complete period objects unchanged')


if __name__ == '__main__':
    main()
