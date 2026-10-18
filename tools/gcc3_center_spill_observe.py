#!/usr/bin/env python3
"""Observe CenterAdapt's allocated but unused spill home in installed Gentoo cc1.

This is a read-only, frozen-revision observation, not a source-spelling search.
The existing whole-TU replay and stack observer provide compilation/identity
controls. This adds only emission/deletion hooks for the four history pseudos.
A successful no-target compilation must fail observation and preserve its object.
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
from gcc3_reload_trace import expressions, register, stack_home
import re
from gcc3_stage_divergence import chunks, instructions, sets

ROOT = Path(__file__).resolve().parents[1]
REV = '4a6519cba'
COMPILER = '80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc'
OBJECT = '2f3918bcc7f7da793ec002c1aba00df7c1d9b2f5607b4f7924b2d3f1cfd1f905'
# Source-history sign-extension, destructive multiply, consuming subtraction,
# pseudo, generated output copies. These are actual UIDs, not line numbers.
ROWS = [(64, 65, 66, 75, [262, 263]), (83, 84, 85, 89, [265, 266]),
        (124, 125, 126, 104, [268, 269]), (143, 144, 145, 118, [271, 272])]


def validate(stack, trace):
    assert stack['exit_codes'] == trace['exit_codes'] == [0]
    assert not stack['errors'] and not trace['errors']
    assert stack['pending'] == trace['pending'] == 0
    slots = [e for e in stack['events'] if e['size']]
    assert [(e['mode'], e['size'], e['frame_before'], e['frame_after']) for e in slots] == [
        ('SImode', 4, 0, -4), ('SImode', 4, -4, -8),
        ('SImode', 4, -8, -12), ('SImode', 4, -12, -16)]
    assert [e['caller_values'][0]['available_values']['i'] for e in slots] == [59, 65, 66, 75]
    assert slots[-1]['caller_values'][0]['available_values']['from_reg'] == 0
    assert 'finish_spills' in slots[-1]['callers']
    assert len(stack['events']) == 6 and len(stack['frames']) == 19
    assert [(e['mode'], e['size'], e['frame_before'], e['frame_after']) for e in stack['events'] if not e['size']] == [
        ('BLKmode', 0, -12, -12), ('BLKmode', 0, -16, -16)]
    assert all(e['layout']['padding1'] == e['layout']['padding2'] == 0 and e['outgoing_args_size'] == 0 for e in stack['frames'])
    final = stack['frames'][-1]
    assert final['locals_size'] == final['layout']['to_allocate'] == 16
    assert final['layout']['padding1'] == final['layout']['padding2'] == 0
    assert final['outgoing_args_size'] == 0 and final['layout']['nregs'] == 4
    assert final['reload_completed'] == 1
    emitted = {e['returned']['uid']: e for e in trace['emissions']}
    assert len(emitted) == len(trace['emissions']) == 8
    deleted = {e['before']['uid']: e for e in trace['events'] if e['hook'] == 'delete_insn'}
    calls = {e['before']['uid']: e for e in trace['events'] if e['hook'] == 'delete_output_reload'}
    for load, multiply, subtract, pseudo, uids in ROWS:
        for uid in [multiply, subtract]:
            event = calls[uid]
            assert 'emit_reload_insns' in event['callers']
            for home in event['history_homes'].values():
                address = home['memory_loc']['operands'][0]
                assert address['code'] == 'PLUS'
                assert address['operands'][0]['regno'] == 20
                assert address['operands'][1]['value'] == -16
                assert home['mem']['operands'][0] == {'code': 'REG', 'mode': 'SImode', 'regno': 7}
        for index, uid in enumerate(uids):
            emission, deletion = emitted[uid], deleted[uid]
            assert 'emit_reload_insns' in emission['callers'] and 'emit_reload_insns' in deletion['callers']
            assert emission['reload_insn_uid'] == [load, multiply][index]
            assert deletion['reload_insn_uid'] == [multiply, subtract][index]
            assert emission['pattern'] == deletion['before']['pattern']
            operands = emission['pattern']['operands']
            assert [v['regno'] for v in operands] == [pseudo, 0]
    assert set(emitted) == {uid for row in ROWS for uid in row[-1]}


def run(command, folder, label, env=None):
    with (folder / (label + '.log')).open('w') as log:
        return subprocess.run(command, cwd=folder, env=env, stdout=log, stderr=subprocess.STDOUT).returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reproduction-dir', type=Path)
    ap.add_argument('--baseline-dir', type=Path, default=ROOT / 'build/tc_repro')
    ap.add_argument('--out', type=Path, default=ROOT / 'build/gcc3-center-spill-observed')
    args = ap.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if args.reproduction_dir:
        source = args.reproduction_dir.resolve()
    else:
        d.REV, d.OUT_NAME = REV, out.name + '-reproduction'
        d.SOURCE_PATHS, d.DUMP_FLAGS = ('src/pump/v34/v34filters.c',), ('-v', '-save-temps', '-da')
        d.variants = lambda path, text: {'baseline': text}
        sys.argv = [sys.argv[0], '--domain', 'unchanged-center-spill-observation',
                    '--baseline-dir', str(args.baseline_dir.resolve())]
        d.main()
        source = ROOT / 'build' / d.OUT_NAME
    prior = json.loads((source / 'results.json').read_text())
    assert prior['revision'].startswith(REV)
    cell = prior['families']['v34filters']['cells']['baseline']
    inputs = source / 'v34filters/baseline'
    assert cell['baseline_reproduced'] and cell['object_hash'] == OBJECT
    assert hashlib.sha256((inputs / 'candidate.o').read_bytes()).hexdigest() == OBJECT
    assert d.tc.REPRODUCE_BUGS in shlex.split(cell['command'][-1])
    stack_dir = out / 'stack'
    command = [sys.executable, str(ROOT / 'tools/gcc3_uref_stack_observe.py'),
               '--reproduction-dir', str(source), '--target', 'V34EqualizerCenterAdapt',
               '--compiler-kind', 'cc1', '--family', 'v34filters', '--generic-trace',
               '--output-dir', str(stack_dir)]
    assert run(command, out, 'stack-driver') == 0
    stack_report = json.loads((stack_dir / 'results.json').read_text())
    assert stack_report['compiler_sha256'] == COMPILER
    assert stack_report['cells']['baseline']['raw_object_unchanged']
    compiler = stack_dir / 'cc1'
    assert hashlib.sha256(compiler.read_bytes()).hexdigest() == COMPILER
    lines = (inputs / 'compile.log').read_text().splitlines()
    cc = shlex.split(next(x for x in lines if '/cc1 -fpreprocessed' in x))
    cc[0], cc[cc.index('-o') + 1] = str(compiler), 'observed.s'
    assembler = shlex.split(next(x for x in lines if '/bin/as ' in x))
    image = prior['config'].splitlines()[0].split(' ', 1)[1]
    reports = {}
    for label, target in [('baseline', 'V34EqualizerCenterAdapt'), ('absent-target', 'NO_SUCH_CENTER_ADAPT')]:
        folder = out / label
        folder.mkdir(exist_ok=True)
        shutil.copy2(inputs / 'v34filters.i', folder / 'v34filters.i')
        debugger = ['gdb', '-q', '-batch', '-ex', 'set pagination off', '-ex',
                    'source ' + str(ROOT / 'tools/gcc3_center_spill_gdb.py'), '--args'] + cc
        status = run(debugger, folder, 'gdb', dict(os.environ, GCC3_CENTER_DELETE_TARGET=target))
        assert (folder / 'observed.s').read_bytes() == (inputs / 'v34filters.s').read_bytes()
        trace = json.loads((folder / 'delete-observe.json').read_text())
        assert trace['exit_codes'] == [0] and not trace['errors'] and not trace['pending']
        assert (status == 0) if label == 'baseline' else (status != 0)
        if label == 'absent-target':
            assert not trace['events'] and not trace['emissions']
        virtual = '/work/' + label
        assemble = list(assembler)
        assemble[assemble.index('-o') + 1], assemble[-1] = virtual + '/observed.o', 'observed.s'
        assemble = d.tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
                    'cd ' + shlex.quote(virtual) + ' && ' + shlex.join(assemble)]
        assert run(assemble, folder, 'assembler') == 0
        assert (folder / 'observed.o').read_bytes() == (inputs / 'candidate.o').read_bytes()
        reports[label] = {'commands': [debugger, assemble], 'observer_exit': status,
                          'compiler_exit': 0, 'raw_object_unchanged': True, 'trace': trace}
    stack, trace = stack_report['cells']['baseline']['trace'], reports['baseline']['trace']
    validate(stack, trace)
    for field in ['missing-emission', 'padding', 'wrong-uid-origin']:
        a, b = copy.deepcopy(stack), copy.deepcopy(trace)
        if field == 'missing-emission':
            b['emissions'].pop()
        elif field == 'padding':
            a['frames'][-1]['layout']['padding2'] = 4
        else:
            b['emissions'][0]['reload_insn_uid'] += 1
        try:
            validate(a, b)
        except (AssertionError, KeyError):
            pass
        else:
            raise AssertionError('negative control accepted: ' + field)
    stages = {}
    for stage in ['01.rtl', '21.ce2', '22.regmove', '24.lreg', '25.greg']:
        streams = chunks(inputs / ('v34filters.c.' + stage), 'V34EqualizerCenterAdapt')
        assert len(streams) == 1
        stages[stage] = instructions(streams[0])
    for index, (load, multiply, subtract, pseudo, copies) in enumerate(ROWS):
        assert register(stages['01.rtl'][load][1]) == pseudo
        for stage in ['01.rtl', '21.ce2']:
            assert register(sets(stages[stage][multiply])[0][1]) == [77, 91, 106, 120][index]
        assert register(sets(stages['22.regmove'][multiply])[0][1]) == pseudo
        assert register(sets(stages['24.lreg'][multiply])[0][1]) == pseudo
        assert not set(copies) & set(stages['24.lreg'])
        assert not set(copies) & set(stages['25.greg'])
    text = chunks(inputs / 'v34filters.c.21.ce2', 'V34EqualizerCenterAdapt')[0]
    roots = expressions(text[re.search(r'^\(insn', text, re.M).start():])
    for _, multiply, _, pseudo, _ in ROWS:
        instruction = next(n for n in roots if n and str(n[0]).startswith('insn') and int(n[1]) == multiply)
        assert any(n and n[0] == 'expr_list:REG_DEAD' and register(n[1]) == pseudo for n in walk(instruction))
    assert not any(stack_home(n) == {'base': 'sp', 'offset': 0, 'mode': 'SI'}
                   for p in stages['25.greg'].values() for n in walk(p))
    result = {'revision': prior['revision'], 'config': prior['config'], 'compiler_sha256': COMPILER,
              'object_sha256': OBJECT, 'rows': ROWS, 'observations': reports,
              'stack': stack_report, 'first_destructive_retargeting': '22.regmove', 'dying_history_notes_before_retargeting': 4, 'controls': {'absent_target_rejected_with_compiler_success': True,
              'changed_padding_rejected': True, 'missing_emission_rejected': True, 'wrong_uid_origin_rejected': True},
              'production_source_changed': False}
    (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('CenterAdapt: 4 shared-home history pseudos; 8 emitted/deleted output copies; 0 surviving home accesses; 4/4 negative controls; both complete objects unchanged')


def walk(node):
    if isinstance(node, list):
        yield node
        for child in node:
            yield from walk(child)


if __name__ == '__main__':
    main()
