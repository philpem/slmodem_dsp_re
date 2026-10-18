#!/usr/bin/env python3
"""Audit the first-CSE member-read substitution in the complete period Echo TU.

Two cells contain identical executable source: the frozen baseline and an
accurate diagnostic comment. This is a stage observation, not a source/profile
recovery. No compiler options, declarations or executable statements change.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import playbook_small_patterns as d
from gcc3_reload_trace import expressions, instructions, register
from gcc3_stage_divergence import chunks
from gcc3_value_carriers_audit import inspect
from gcc_x87_eia6_mechanism import deaths
from recovery_fax_debug_audit import Object

REV = '51f62d213'
SOURCE = 'src/pump/v90/V92EchoCanceller.cpp'
PREFIX = '_ZN16V92EchoCanceller'
DUMPS = ('-v', '-save-temps', '-da', '-fdump-tree-original', '-fdump-tree-inlined')
STAGES = ('01.rtl', '02.sibling', '04.jump', '05.null', '06.cse',
          '18.cse2', '19.life', '20.combine', '22.regmove', '24.lreg',
          '25.greg', '33.sched2', '34.stack')
SETTERS = {'setEchoBeta': ('echoBeta', 48, PREFIX + '11setEchoBetaEf'),
           'setDecayFactor': ('echoBetaDecay', 52, PREFIX + '14setDecayFactorEf')}
OLD = ''' * register for the magnitude and `fcomps 0x30(%ebx)` reads the store back for
 * the sign.  They are the same float; the spelling is the object's.'''
NEW = ''' * register for the magnitude and `fcomps 0x30(%ebx)` reads the store back for
 * the sign in the reference.  The source names those operands, but retained
 * Gentoo first RTL CSE forwards the stored argument into the sign comparison:
 * the standalone setters retain an x87 value and emit `fcompp`, not a member
 * readback.  Their 149-versus-147-byte residual also includes product/pop and
 * scheduling differences; it is not just a two-byte reload discrepancy.'''


def variants(path, source):
    assert path == SOURCE and source.count(OLD) == 1
    return {'baseline': source, 'comment': source.replace(OLD, NEW)}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def walk(node):
    if isinstance(node, list):
        yield node
        for child in node:
            yield from walk(child)


def operations(node, code):
    return [value for value in walk(node) if value and value[0] == code]


def member(node, field, offset):
    return (isinstance(node, list) and node and node[0] == 'mem/s:SF' and
            '<variable>.' + field + '+0' in node and
            any(value[:2] == ['const_int', str(offset)] for value in walk(node)))


def stage(folder, method, suffix):
    header = 'void V92EchoCanceller::' + method
    header += '(float)' if method in SETTERS else '(V92EchoCancellerState)' if method == 'setState' else '()'
    path = folder / ('V92EchoCanceller.cpp.' + suffix)
    text = chunks(path, header)
    assert len(text) == 1
    text = text[0]
    patterns = instructions(text)
    start = re.search(r'^\((?:insn|jump_insn|call_insn)(?::\S+)? ', text, re.M)
    nodes = {int(node[1]): node for node in expressions(text[start.start():])
             if node and re.fullmatch(r'(?:insn|jump_insn|call_insn)(?::\S+)?', node[0])}
    assert set(patterns) == set(nodes)
    return {'sha256': sha(path), 'instruction_count': len(patterns),
            'patterns': patterns, 'deaths': {uid: deaths(node) for uid, node in nodes.items()}}


def verify_setter(streams, field, offset):
    # These are actual UID/value chains in the pinned complete TU, not a
    # generic claim that every possible spelling must receive this transform.
    for suffix in STAGES[:4]:
        p = streams[suffix]['patterns']
        assert register(p[4][1]) == 59 and p[4][1][0].endswith(':SF')
        assert member(p[14][1], field, offset) and register(p[14][2]) == 59
        assert register(p[75][1]) == 74 and member(p[75][2], field, offset)
        assert any(register(n) == 74 for n in walk(p[83]))
    for suffix in STAGES[4:]:
        p = streams[suffix]['patterns']
        assert 75 not in p
        assert not any(member(n, field, offset) for uid, pattern in p.items()
                       if uid != 14 for n in walk(pattern))
    p = streams['06.cse']['patterns']
    assert register(p[14][2]) == 59
    assert any(register(n) == 59 for n in walk(p[83]))
    assert not any(register(n) == 74 for n in walk(p[83]))
    for uid in (27, 28, 60):
        assert any(register(n) == 59 for n in walk(p[uid]))
    # Subtraction input extension folds at combine. The scale remains a
    # separate XF register through regmove, then becomes an SF memory operand
    # by local allocation. This is independent of the earlier sign forwarding.
    assert not operations(streams['19.life']['patterns'][30], 'float_extend:XF')
    assert operations(streams['20.combine']['patterns'][30], 'float_extend:XF')
    assert not any(n and isinstance(n[0], str) and n[0].startswith('mem') for n in walk(streams['22.regmove']['patterns'][32]))
    assert any(n and n[0] == 'mem/u/f:SF' for n in walk(streams['24.lreg']['patterns'][32]))
    for suffix in ('18.cse2', '19.life', '20.combine', '22.regmove', '24.lreg'):
        compare = operations(streams[suffix]['patterns'][121], 'compare:CCFP')
        assert len(compare) == 1 and register(compare[0][2]) == 59
        assert not any(n and isinstance(n[0], str) and n[0].startswith('mem') for n in walk(compare))
    before, after = streams['33.sched2'], streams['34.stack']
    assert operations(before['patterns'][121], 'compare:CCFP')
    assert operations(after['patterns'][121], 'compare:CCFP')
    assert len(after['deaths'][121]) == 2
    assert not before['deaths'][30] and not after['deaths'][30]
    # Final stack conversion inserts FXCH between subtraction and memory
    # multiply while the sign's original value remains live. No claim that
    # this alone establishes the cause of every native scheduling difference.
    return {'store_uid': 14, 'argument_pseudo': 59, 'sign_load_uid': 75,
            'sign_pseudo': 74, 'branch_uid': 83, 'lowered_compare_uid': 121,
            'first_missing_load_stage': '06.cse',
            'first_memory_scale_stage_in_checked_boundary': '24.lreg'}


def tree_read(folder, method, field, suffix):
    path = folder / ('V92EchoCanceller.cpp.' + suffix)
    text = path.read_text()
    blocks = re.split(r'^;; Function ', text, flags=re.M)[1:]
    selected = [block for block in blocks if block.splitlines()[0].startswith('void V92EchoCanceller::' + method + '(float)')]
    assert len(selected) == 1
    nodes = {int(number): body for number, body in re.findall(r'^@(\d+)\s+(.*?)(?=^@\d+|\Z)', selected[0], re.M | re.S)}
    fields = {number for number, body in nodes.items() if body.startswith('field_decl') and
              any(nodes[int(ref)].startswith('identifier_node') and
                  re.search(r'\bstrg: ' + field + r'\s', nodes[int(ref)])
                  for ref in re.findall(r'name: @(\d+)', body))}
    reads = {number for number, body in nodes.items() if body.startswith('component_ref') and
             any(int(ref) in fields for ref in re.findall(r'op 1: @(\d+)', body))}
    initializers = {number for number, body in nodes.items() if body.startswith('var_decl') and
                    any(int(ref) in reads for ref in re.findall(r'init: @(\d+)', body))}
    assert len(fields) == len(initializers) == 1 and len(reads) == 2
    return {'sha256': sha(path), 'field_nodes': sorted(fields), 'component_nodes': sorted(reads),
            'inlined_parameter_initializer_nodes': sorted(initializers)}


def audit(baseline):
    root = d.ROOT / 'build' / d.OUT_NAME
    ledger = json.loads((root / 'results.json').read_text())
    manifest = json.loads((baseline / 'manifest.json').read_text())
    assert manifest['revision'].startswith(REV) and len(manifest['objects']) == 300
    assert ledger['config'] == manifest['config'] == (baseline / '.build-config').read_text()
    assert ledger['revision'] == REV
    assert all(sha(d.ROOT / path) == digest for path, digest in ledger['headers'].items())
    source = subprocess.check_output(['git', 'show', REV + ':' + SOURCE], cwd=d.ROOT, text=True)
    family = ledger['families']['V92EchoCanceller']
    expected = variants(SOURCE, source)
    assert set(family['cells']) == set(expected)
    folder = root / 'V92EchoCanceller'
    frozen = baseline / (SOURCE.replace('/', '_') + '.o')
    assert sha(frozen) == manifest['objects'][frozen.name]
    report = {'revision': manifest['revision'], 'baseline_directory': str(baseline),
              'config': ledger['config'], 'manifest_sha256': sha(baseline / 'manifest.json'), 'cells': {}}
    for label, cell in family['cells'].items():
        directory = folder / label
        obj = directory / 'candidate.o'
        assert obj.read_bytes() == frozen.read_bytes()
        assert sha(obj) == cell['object_hash']
        assert (directory / 'V92EchoCanceller.cpp').read_text() == expected[label]
        assert sha(directory / 'V92EchoCanceller.cpp') == cell['source_hash']
        args = shlex.split(cell['command'][-1]); args = args[args.index('g++') + 1:]
        lines = ledger['config'].splitlines()
        flags = shlex.split(next(line[6:] for line in lines if line.startswith('flags ')))
        flags = ['-I/src/include' if word == '-Iinclude' else '/src/' + word
                 if word == 'tools/toolchain/period_compat.h' else word for word in flags]
        flags += list(DUMPS) + shlex.split(next(line[6:] for line in lines if line.startswith('cxx   ')))
        virtual = '/work/V92EchoCanceller/' + label
        assert args == ['-c'] + flags + [d.tc.REPRODUCE_BUGS, '-o', virtual + '/candidate.o', virtual + '/V92EchoCanceller.cpp']
        assert not cell['diagnostic_flags']
        log = (directory / 'compile.log').read_text()
        assert 'GNU C++ version 3.4.2  (Gentoo Linux 3.4.2-r2' in log
        assert 'GNU assembler version 2.15.92.0.2 (i386-pc-linux-gnu)' in log
        sizes = d.b.sizes(str(obj))
        verdicts = {name: list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))) for name in sizes}
        assert len(sizes) == 15 and verdicts == cell['verdicts'] and sorted(sizes) == cell['functions']
        assert sum(value[0] == 'EXACT' for value in verdicts.values()) == 8
        assert all(verdicts[PREFIX + 'C%dEP13V92Parametersjj' % n] == ['EXACT', 0] for n in (1, 2))
        assert Object(obj).canonical() == Object(frozen).canonical() and inspect(obj) == inspect(frozen)
        assert not cell.get('gains') and not cell.get('losses')
        row = {'object_sha256': sha(obj), 'actual_command': cell['command'], 'verdicts': verdicts,
               'sizes': sizes, 'gains': cell.get('gains', []), 'losses': cell.get('losses', []), 'traces': {}}
        for method, (field, offset, symbol) in SETTERS.items():
            streams = {suffix: stage(directory, method, suffix) for suffix in STAGES}
            links = verify_setter(streams, field, offset)
            tree = {suffix: tree_read(directory, method, field, suffix) for suffix in ('original', 'inlined')}
            native = d.b.insns(d.b.BLOB, symbol)
            current = d.b.insns(str(obj), symbol)
            assert [(m, o) for m, o in native if m.startswith('fcom')] == [('fcomps', '0x%x(%%ecx)' % offset)]
            assert [(m, o) for m, o in current if m.startswith('fcom')] == [('fcompp', '')]
            assert [m for m, o in native if m.startswith('fmul')] == ['fmulp']
            assert [m for m, o in current if m.startswith('fmul')] == ['fmuls']
            assert sizes[symbol] == 149 and d.b.sizes(d.b.BLOB)[symbol] == 147
            row['traces'][method] = {'links': links, 'tree': tree, 'stages': streams,
                                     'native': native, 'candidate': current}
        # Independent consumer witnesses: all six sign field reads in setState
        # disappear at first CSE, while reset already has literal signs natively.
        consumers = {method: {suffix: stage(directory, method, suffix)
                             for suffix in ('01.rtl', '05.null', '06.cse')}
                     for method in ('setState', 'reset')}
        state_before = consumers['setState']['05.null']['patterns']
        state_after = consumers['setState']['06.cse']['patterns']
        loads = {uid: pattern for uid, pattern in state_before.items() if pattern[0] == 'set' and
                 any(member(pattern[2], field, offset) for field, offset, symbol in SETTERS.values())}
        assert sorted(loads) == [260, 367, 519, 627, 760, 868]
        assert not set(loads) & set(state_after)
        for uid in loads:
            assert consumers['setState']['01.rtl']['patterns'][uid] == loads[uid]
            assert any(register(n) == register(loads[uid][1]) for n in walk(state_before[uid + 8]))
        assert 268 not in state_after and 375 not in state_after
        for branch, forwarded in ((527, 129), (635, 150), (768, 173), (876, 194)):
            assert any(register(n) == forwarded for n in walk(state_after[branch]))
        for method, before in ((method, streams['05.null']['patterns']) for method, streams in consumers.items()):
            after = consumers[method]['06.cse']['patterns']
            field_loads = {uid: p for uid, p in before.items() if p[0] == 'set' and
                           any(member(p[2], field, offset) for field, offset, symbol in SETTERS.values())}
            assert len(field_loads) == (6 if method == 'setState' else 2)
            assert not set(field_loads) & set(after)
            assert not any(member(n, field, offset) for pattern in after.values()
                           if pattern[0] == 'set' for n in walk(pattern[2])
                           for field, offset, symbol in SETTERS.values())
        reset_before = consumers['reset']['05.null']['patterns']
        reset_after = consumers['reset']['06.cse']['patterns']
        for load, branch, constant in ((200, 208, 216), (307, 315, 323)):
            assert load in reset_before and branch in reset_before
            assert load not in reset_after and branch not in reset_after
            assert reset_after[constant][2][:2] == ['const_int', '45']
        native_state = d.b.insns(d.b.BLOB, PREFIX + '8setStateE21V92EchoCancellerState')
        field_compares = [(m, o) for m, o in native_state if m.startswith('fcom') and o in ('0x30(%ebx)', '0x34(%ebx)')]
        assert len(field_compares) == 6
        reset = PREFIX + '5resetEv'
        assert verdicts[reset] == ['EXACT', 0] and sizes[reset] == 195
        native_reset = d.b.insns(d.b.BLOB, reset)
        assert not any(m.startswith('fcom') for m, o in native_reset)
        assert sum(m == 'mov' and o == '$0x2d,%eax' for m, o in native_reset) == 2
        row['consumer_controls'] = {'streams': consumers, 'setState_removed_loads': loads, 'native_setState_field_compares': field_compares,
                                     'native_reset_literal_signs': 2, 'native_reset_field_compares': 0}
        report['cells'][label] = row
    controls = []
    target = report['cells']['baseline']['traces']['setDecayFactor']['stages']
    def wrong_forwarded_value(streams):
        operands = [node for node in walk(streams['06.cse']['patterns'][83]) if register(node) == 59]
        assert len(operands) == 1
        operands[0][1] = '74'
    for label, mutate in (
        ('missing_initial_load', lambda s: s['01.rtl']['patterns'].pop(75)),
        ('wrong_member_offset', lambda s: s['05.null']['patterns'][75][2][1][2].__setitem__(1, '48')),
        ('wrong_forwarded_value', wrong_forwarded_value),
        ('retained_reload', lambda s: s['06.cse']['patterns'].__setitem__(75, s['01.rtl']['patterns'][75])),
        ('wrong_scale_mode', lambda s: s['24.lreg']['patterns'][32].__setitem__(2, ['mult:XF', ['reg:XF', '64'], ['reg:XF', '66']])),
        ('missing_compare_death', lambda s: s['34.stack']['deaths'].__setitem__(121, [])),
    ):
        changed = copy.deepcopy(target)
        mutate(changed)
        try:
            verify_setter(changed, 'echoBetaDecay', 52)
        except (AssertionError, KeyError):
            controls.append(label)
        else:
            raise AssertionError('corrupted trace accepted: ' + label)
    report['controls'] = controls
    report['body_verdicts_checked'] = 30
    report['setter_stage_streams_checked'] = len(STAGES) * 4
    report['executable_source_changed'] = False
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('2 raw-equal complete TUs /30 body verdicts /%d setter stage streams; 6 corrupted-trace controls rejected' % report['setter_stage_streams_checked'])
    print('Both sign member loads disappear at first CSE; scale folds by local allocation; reset provides native literal-sign counterexample')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir', required=True, type=Path,
                        help='frozen 51f62d213 canonical objects, config and manifest')
    parser.add_argument('--domain', default='first-cse-member-readback', help='recorded experiment label')
    parser.add_argument('--audit', action='store_true', help='rescore existing artifacts without compiling')
    args = parser.parse_args()
    d.REV, d.OUT_NAME, d.SOURCE_PATHS, d.DUMP_FLAGS, d.variants = REV, 'echo-decay-lifetime', (SOURCE,), DUMPS, variants
    if args.audit:
        audit(args.baseline_dir.resolve())
    else:
        baseline = args.baseline_dir.resolve()
        sys.argv = [sys.argv[0], '--domain', args.domain, '--baseline-dir', str(baseline)]
        d.main()
        audit(baseline)
