#!/usr/bin/env python3
"""Trace preinitdigital allocation/renaming without changing period output.

No source variants. Baseline and absent-target observations must both preserve
the complete object; the latter must fail the nonempty-target detector.
"""
import argparse
import copy
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import playbook_small_patterns as d

ROOT = d.ROOT
REV = '8e216110a'
COMPILER = '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc'
OBJECT = '70b3c7724acc412edc9e78309233981bc8c11c869029e9ba4b93f40cb62b2c18'


def validate(trace):
    assert trace['exit_codes'] == [0] and not trace['errors'] and trace['pending'] == 0
    events = trace['events']
    choices = [e for e in events if e['kind'] == 'local_choice']
    assert len(events) == 37 and len(choices) == 9
    assert [e['members'] for e in choices] == [[r] for r in (67, 69, 76, 78, 79, 80, 81, 82, 83)]
    assert [e['return'] for e in choices] == [0] * 8 + [1]
    for row in choices:
        assert row['args']['class'] == 12 and row['args']['mode'] == 12
        assert not row['copy_suggestions'] and not row['arithmetic_suggestions']
        assert row['allocation_order'][:3] == [0, 1, 2]
        assert row['quantity']['n_calls_crossed'] == 0
        used = row['exclusions'][0]['used']
        assert (0 in used) == (row['members'] == [83])
        assert 1 not in used and 2 not in used
    for pseudo, refs, birth, death in ((79, 2, 20, 22), (80, 2, 30, 32),
                                      (81, 2, 40, 42), (82, 5, 46, 60), (83, 4, 48, 68)):
        row = next(e for e in choices if e['members'] == [pseudo])
        assert [row['quantity'][k] for k in ('n_refs', 'birth', 'death')] == [refs, birth, death]
    homes = {str(r): 0 if r != 83 else 1 for r in range(79, 84)}
    local = next(e for e in events if e['kind'] == 'local_alloc')
    global_ = next(e for e in events if e['kind'] == 'global_alloc')
    assert set(local['homes_before'].values()) == {-1} and local['homes_after'] == homes
    assert global_['homes_before'] == global_['homes_after'] == homes
    renames = [e for e in events if e['kind'] == 'rename']
    assert len(renames) == 26
    for uid, selected in ((390, 1), (391, 1), (392, 1), (423, 0), (427, 0),
                          (431, 0), (425, 2), (429, 2), (433, 2), (394, 1)):
        row = next(e for e in renames if e['chain'][0]['uid'] == uid)
        assert row['selected'] == selected and row['old'] == (1 if uid == 394 else 0)
        if uid == 394:
            assert all(r in row['unavailable'] for r in (0, 1, 2, 3))
            assert [r['uid'] for r in row['chain']] == [394, 235, 239, 241]
        else:
            assert row['tick'][selected] == min(row['tick'][:3])


def validate_scratch(report):
    trace = report['cells']['v34shell/baseline']['trace']
    assert trace['exit_codes'] == [0] and not trace['errors'] and not trace['pending']
    assert len(trace['events']) == 100
    target = [e for e in trace['events'] if e['function'] == 'preinitdigital']
    assert len(target) == 23
    for uid in (199, 200, 205, 206, 211, 212):
        row = next(e for e in target if e['insn_uid'] == uid)
        assert row['selected'] == 0 and row['eligibility']['live'] == [3, 7]
    assert report['cells']['v34shell/baseline']['raw_object_unchanged']


def audit(out):
    report = json.loads((out / 'results.json').read_text())
    assert report['compiler_sha256'] == COMPILER and report['object_sha256'] == OBJECT
    inputs = Path(report['inputs'])
    assert hashlib.sha256((out / 'cc1').read_bytes()).hexdigest() == COMPILER
    assert hashlib.sha256((inputs / 'candidate.o').read_bytes()).hexdigest() == OBJECT
    assert stage_audit(inputs) == report['stages']
    validate(report['cells']['baseline']['trace'])
    absent = report['cells']['absent-target']
    assert absent['debugger_exit'] != 0
    assert absent['trace'] == {'target': 'NO_SUCH_PREINIT', 'events': [], 'errors': [], 'exit_codes': [0], 'pending': 0}
    for label, row in report['cells'].items():
        assert row['raw_object_unchanged'] and row['assembly_unchanged']
        assert (out / label / 'observed.o').read_bytes() == (inputs / 'candidate.o').read_bytes()
        assert (out / label / 'observed.s').read_bytes() == (inputs / 'v34shell.s').read_bytes()
    validate_scratch(report['scratch'])
    assert (out / 'scratch/v34shell-baseline/observed.o').read_bytes() == (inputs / 'candidate.o').read_bytes()
    assert report['stages']['class_costs_equal_gprs']
    wrong = copy.deepcopy(report['cells']['baseline']['trace'])
    next(e for e in wrong['events'] if e.get('members') == [83])['return'] = 2
    try:
        validate(wrong)
    except AssertionError:
        pass
    else:
        raise AssertionError('wrong allocation negative did not fire')
    print('allocation audit: 9 local choices, 26 rename decisions; 100 scratch searches/23 target; '
          '2 raw objects; absent-target and wrong-allocation controls pass')


def stage_audit(inputs):
    def section(stage):
        return (inputs / ('v34shell.c.' + stage)).read_text().split(
            ';; Function preinitdigital', 1)[1].split(';; Function ', 1)[0]
    def insns(stage):
        return {int(m[1]): m[0] for m in re.finditer(
            r'^\(insn(?::\w+)? (\d+)[\s\S]*?(?=^\(|\Z)', section(stage), re.M)}
    costs = {}
    text = section('24.lreg')
    for pseudo in range(79, 84):
        rows = re.findall(r'  Register %d costs: ([^\n]+)' % pseudo, text)
        assert len(rows) == 2 and rows[0] == rows[1]
        row = dict((name, int(cost)) for name, cost in re.findall(r'(\w+):(-?\d+)', rows[0]))
        assert all(row[k] == 0 for k in ('AREG', 'DREG', 'CREG', 'GENERAL_REGS'))
        costs[str(pseudo)] = row
    flow = insns('27.flow2')
    for uid, constant in ((199, 32), (200, 0), (205, 1024), (206, 0), (211, 384), (212, 0)):
        assert flow[uid].count('(set (mem/f:SI') == 1
        assert '(const_int %d ' % constant in flow[uid]
    for stage, expected in (('28.peephole2', (0, 0, 0)), ('29.ce3', (0, 0, 0)),
                            ('30.rnreg', (1, 0, 2))):
        blocks = insns(stage)
        for uid in (390, 391, 392):
            assert re.search(r'\(set \(reg(?:/f)?:SI %d ' % expected[0], blocks[uid])
        for uid in (423, 427, 431):
            assert '(set (reg:SI %d ' % expected[1] in blocks[uid]
        for uid in (425, 429, 433):
            assert '(set (reg:SI %d ' % expected[2] in blocks[uid]
    return {'class_costs_equal_gprs': True, 'class_costs': costs,
            'memset_constant_uids_through_flow2': [199, 200, 205, 206, 211, 212],
            'argument_registers_destination_zero_size': {
                'peephole2_and_ce3': [0, 0, 0], 'rnreg': [1, 0, 2]}}


def run(command, folder, label, env=None):
    with (folder / (label + '.log')).open('w') as log:
        return subprocess.run(command, cwd=folder, env=env, stdout=log, stderr=subprocess.STDOUT).returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reproduction-dir', type=Path)
    ap.add_argument('--baseline-dir', type=Path, default=ROOT / 'build/tc_repro')
    ap.add_argument('--out', type=Path, default=ROOT / 'build/preinit-alloc-observed')
    ap.add_argument('--audit', action='store_true')
    args = ap.parse_args()
    out = args.out.resolve()
    if args.audit:
        audit(out)
        return
    out.mkdir(parents=True, exist_ok=True)
    if args.reproduction_dir:
        source = args.reproduction_dir.resolve()
    else:
        d.REV, d.OUT_NAME = REV, out.name + '-reproduction'
        d.SOURCE_PATHS, d.DUMP_FLAGS = ('src/pump/v34/v34shell.c',), ('-v', '-save-temps', '-da')
        d.variants = lambda path, text: {'baseline': text}
        sys.argv = [sys.argv[0], '--domain', 'unchanged-preinit-allocation-observation',
                    '--baseline-dir', str(args.baseline_dir.resolve())]
        d.main()
        source = ROOT / 'build' / d.OUT_NAME
    prior = json.loads((source / 'results.json').read_text())
    assert prior['revision'].startswith(REV)
    cell = prior['families']['v34shell']['cells']['baseline']
    inputs = source / 'v34shell/baseline'
    assert cell['baseline_reproduced'] and cell['object_hash'] == OBJECT
    assert hashlib.sha256((inputs / 'candidate.o').read_bytes()).hexdigest() == OBJECT
    assert d.tc.REPRODUCE_BUGS in shlex.split(cell['command'][-1])
    lines = (inputs / 'compile.log').read_text().splitlines()
    compiler = shlex.split(next(line for line in lines if '/cc1 -fpreprocessed' in line))
    assembler = shlex.split(next(line for line in lines if '/bin/as ' in line))
    image = prior['config'].splitlines()[0].split(' ', 1)[1]
    executable = out / 'cc1'
    with executable.open('wb') as stream:
        subprocess.run(['docker', 'run', '--rm', '--platform', 'linux/386', image, 'cat', compiler[0]], stdout=stream, check=True)
    executable.chmod(0o755)
    assert hashlib.sha256(executable.read_bytes()).hexdigest() == COMPILER
    compiler[0], compiler[compiler.index('-o') + 1] = str(executable), 'observed.s'
    report = {'revision': prior['revision'], 'config': prior['config'],
              'compiler_sha256': COMPILER, 'object_sha256': OBJECT,
              'preparation_command': cell['command'], 'source_sha256': cell['source_hash'],
              'header_sha256': prior['headers'],
              'inputs': str(inputs),
              'stages': stage_audit(inputs), 'cells': {}}
    for label, target in (('baseline', 'preinitdigital'), ('absent-target', 'NO_SUCH_PREINIT')):
        folder = out / label
        folder.mkdir(exist_ok=True)
        shutil.copy2(inputs / 'v34shell.i', folder / 'v34shell.i')
        debugger = ['gdb', '-q', '-batch', '-ex', 'set pagination off', '-ex',
                    'source ' + str(ROOT / 'tools/gcc3_preinit_alloc_gdb.py'), '--args'] + compiler
        code = run(debugger, folder, 'gdb', dict(os.environ, GCC3_PREINIT_TARGET=target))
        trace = json.loads((folder / 'allocation-observe.json').read_text())
        assert (code == 0) if label == 'baseline' else (code != 0)
        assert (folder / 'observed.s').read_bytes() == (inputs / 'v34shell.s').read_bytes()
        virtual = '/work/' + label
        assemble = list(assembler)
        assemble[assemble.index('-o') + 1], assemble[-1] = virtual + '/observed.o', 'observed.s'
        assemble = d.tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
                    'cd ' + shlex.quote(virtual) + ' && ' + shlex.join(assemble)]
        assert run(assemble, folder, 'assembler') == 0
        assert (folder / 'observed.o').read_bytes() == (inputs / 'candidate.o').read_bytes()
        report['cells'][label] = {'debugger_exit': code, 'commands': [debugger, assemble],
                                 'raw_object_unchanged': True, 'assembly_unchanged': True, 'trace': trace}
    scratch = out / 'scratch'
    command = [sys.executable, str(ROOT / 'tools/gcc3_mechanism_scratch_observe.py'),
               '--reproduction-dir', str(source), '--target', 'preinitdigital', '--out', str(scratch)]
    assert run(command, out, 'scratch-driver') == 0
    report['scratch_command'] = command
    report['scratch'] = json.loads((scratch / 'results.json').read_text())
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    audit(out)


if __name__ == '__main__':
    main()
