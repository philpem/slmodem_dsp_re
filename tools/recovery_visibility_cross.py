#!/usr/bin/env python3
"""Diagnostic O3 automatic-inline/unit-visibility cross on three complete TUs.

Unchanged source; all four option combinations per TU. No option is adopted.

Next diagnostic, not run here: move the unchanged ordinary setEchoDelay
definition before/after its constructor, crossed with unit visibility. This
tests delay-inline/reset-call availability while retaining its strong export.
Native GLOBAL DEFAULT binding does not establish a header-inline declaration;
an inline spelling that weakens or removes that export would fail the constraint.
"""
import json
import copy
import hashlib
import shlex
import shutil
import subprocess
import sys
import re
from pathlib import Path
from collections import Counter
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from recovery_fax_debug_audit import Object
from recovery_generator_separation import named_data

SOURCES = ('src/pump/v90/V92ConvEncoder.cpp', 'src/pump/v90/V92EchoCanceller.cpp',
           'src/pump/v90/V90Phase4Modulator.cpp')
FLAGS = {'baseline': (), 'auto-off': ('-fno-inline-functions',),
         'unit-off': ('-fno-unit-at-a-time',),
         'both-off': ('-fno-inline-functions', '-fno-unit-at-a-time')}
TARGETS = {
    'V92ConvEncoder': ['_ZN21V92ConvolutionEncoder7processEPi'],
    'V92EchoCanceller': ['_ZN16V92EchoCancellerC1EP13V92Parametersjj',
                         '_ZN16V92EchoCancellerC2EP13V92Parametersjj'],
    'V90Phase4Modulator': ['_ZN18V90Phase4Modulator30recivedPartTwoSilenceRrnSUVtagEv'],
}


def edges(path, symbol):
    return [(op, m[1]) for op, operands in d.b.insns(str(path), symbol)
            if op in ('call', 'jmp')
            for m in [re.search(r"@R_386_PC32:\('symbol', '([^']+)'", operands)] if m]


def frontend_cost(text, signature):
    values = {int(n) for n in re.findall(re.escape(signature) + r': (\d+) insns', text)}
    if len(values) != 1:
        raise ValueError('absent/inconsistent frontend cost: ' + signature)
    return values.pop()


def validate(report):
    enc, echo, mod = ('_ZN21V92ConvolutionEncoder', '_ZN16V92EchoCanceller', '_ZN18V90Phase4Modulator')
    additions = ['_Z13ec_filter_sumPKfS0_j', '_Z7frac_off', '_Z7sign_off', '_Z8whole_off']
    mod_gains = [mod + n for n in ('10recivedE2uEv', '12recivedCPtagEv',
        '12setRfSymbolsEP16V90MappingParams', '14setRdRtSymbolsEP16V90MappingParams',
        '18recivedFirstRrnE2uEv', '30recivedPartTwoSilenceRrnSUVtagEv')]
    changes = {'V92ConvEncoder': [0, 1, 0, 1], 'V92EchoCanceller': [0, 8, 8, 10],
               'V90Phase4Modulator': [0, 10, 5, 11]}
    for family, cells in report['families'].items():
        for index, (label, row) in enumerate(cells.items()):
            auto_off, unit_off = label in ('auto-off', 'both-off'), label in ('unit-off', 'both-off')
            assert row['bindings_equal'] and row['named_data_equal'] and row['nobits_equal']
            assert not row['missing_functions']
            assert len(row['changed_bodies']) == changes[family][index]
            assert row['added_functions'] == (additions if family == 'V92EchoCanceller' and auto_off else [])
            expected_gains, expected_losses, expected_data = [], [], set()
            if family == 'V92ConvEncoder':
                expected_gains = [enc + '7processEPi'] if auto_off else []
                expected_data = {'.data'} if unit_off else set()
                edge = cells[label]['target_edges'][enc + '7processEPi']
                assert list(map(tuple, edge['original'])) == [('call', enc + '10inverseMapEPi')]
                assert list(map(tuple, edge['candidate'])) == ([('call', enc + '10inverseMapEPi')] if auto_off else [])
                assert row['functions'] == 8
                if auto_off:
                    assert row['verdicts'][enc + '7processEPi'] == ['EXACT', 0]
            elif family == 'V92EchoCanceller':
                expected_losses = [echo + '12setEchoDelayEj'] if unit_off else []
                expected_data = ({'.rodata.cst4'} if auto_off else set()) | ({'.rodata.str1.4'} if unit_off else set())
                assert row['functions'] == 15 + (4 if auto_off else 0)
                for edge in row['target_edges'].values():
                    actual, original = Counter(map(tuple, edge['candidate'])), Counter(map(tuple, edge['original']))
                    assert original[('jmp', echo + '5resetEv')] == 1
                    assert original[('call', echo + '12setEchoDelayEj')] == 0
                    assert actual[('jmp', echo + '5resetEv')] == (label != 'baseline')
                    assert actual[('call', echo + '12setEchoDelayEj')] == (0 if label == 'baseline' else 2)
            else:
                expected_gains = (mod_gains if auto_off else []) + (['_ZN9ScramblerIhhEC1Ejjj'] if unit_off else [])
                expected_losses = [mod + '21resetRRNSecondSectionEv'] if unit_off else []
                expected_data = {'.rodata.str1.4'} if unit_off else set()
                edge = row['target_edges'][TARGETS[family][0]]
                assert list(map(tuple, edge['original'])) == [('jmp', mod + '13recivedSUVtagEv')]
                assert list(map(tuple, edge['candidate'])) == ([('jmp', mod + '13recivedSUVtagEv')] if auto_off else [('call', 'edprintf')])
                assert row['functions'] == 53
            assert row['exact_gains'] == expected_gains and row['exact_losses'] == expected_losses
            assert set(row['allocated_noncode_changed']) == expected_data
            assert all(r[0:2] == ['STT_FUNC', 'STB_LOCAL'] for r in row['added_function_records'].values())
            assert not row['added_globals'] and not row['missing_globals']
    assert report['original_function_copy_verdicts'] == 304
    assert report['emitted_function_copies'] == 312


def matrix():
    """Shared-helper baseline, then inventory-permissive diagnostic cells.

    Disabling automatic inline can emit LOCAL helpers absent at baseline.
    Report them explicitly; do not impose a source-recovery inventory freeze.
    """
    root = d.ROOT / 'build' / d.OUT_NAME
    results = json.loads((root / 'results.json').read_text())
    assert results['revision'] == d.REV
    config = results['config']
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/' + f
             if f == 'tools/toolchain/period_compat.h' else f for f in flags]
    cxx = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('cxx   ')))
    original_names = set(d.b.sizes(d.b.BLOB))
    for family, data in results['families'].items():
        folder = root / family
        baseline = folder / 'baseline'
        base = data['cells']['baseline']
        for label, control in FLAGS.items():
            if label == 'baseline':
                continue
            out = folder / label
            out.mkdir(exist_ok=True)
            source = Path(data['source_path']).name
            shutil.copy2(baseline / source, out / source)
            for local in baseline.glob('*.h'):
                shutil.copy2(local, out / local.name)
            virtual = '/work/' + family + '/' + label
            shell = d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH,
                flags + list(d.DUMP_FLAGS) + list(control) + cxx,
                virtual + '/candidate.o', virtual + '/' + source)
            assert shell.count('exec gcc -c ') == 1
            shell = shell.replace('exec gcc -c ', 'exec g++ -c ', 1)
            command = d.tc.docker_prefix(image, d.ROOT, root, True) + ['/bin/sh', '-c',
                'cd ' + shlex.quote(virtual) + ' && ' + shell]
            row = {'source_hash': base['source_hash'], 'command': command, 'diagnostic_flags': list(control)}
            data['cells'][label] = row
            with (out / 'compile.log').open('w') as log:
                row['compile_exit'] = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
            (root / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
            assert row['compile_exit'] == 0, (family, label)
            path = out / 'candidate.o'
            row['object_hash'] = hashlib.sha256(path.read_bytes()).hexdigest()
            row['globals'] = {line.split()[-1]: line.split()[-2] for line in
                subprocess.check_output(['nm', '-g', '--defined-only', str(path)], text=True).splitlines()}
            row['functions'] = sorted(d.b.sizes(str(path)))
            row['added_functions'] = sorted(set(row['functions']) - set(base['functions']))
            row['missing_functions'] = sorted(set(base['functions']) - set(row['functions']))
            row['added_globals'] = sorted(set(row['globals']) - set(base['globals']))
            row['missing_globals'] = sorted(set(base['globals']) - set(row['globals']))
            row['verdicts'] = {n: list(d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(path), n)))
                for n in row['functions'] if n in original_names}
            row['changed_bodies'] = [n for n in row['functions'] if n in base['functions'] and
                d.b.body(str(path), n) != d.b.body(str(baseline / 'candidate.o'), n)]
            row['gains'] = [n for n,v in row['verdicts'].items() if v[0] == 'EXACT' and
                base['verdicts'].get(n, ['MISSING'])[0] != 'EXACT']
            row['losses'] = [n for n,v in base['verdicts'].items() if v[0] == 'EXACT' and
                row['verdicts'].get(n, ['MISSING'])[0] != 'EXACT']
            (root / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
            print(family, label, 'new locals', row['added_functions'], 'gains', row['gains'],
                  'losses', row['losses'], flush=True)


def audit():
    root = d.ROOT / 'build' / d.OUT_NAME
    results = json.loads((root / 'results.json').read_text())
    assert results['revision'] == d.REV
    report = {'families': {}, 'source_cells': 12, 'profile_adoption': False}
    original_sizes = d.b.sizes(d.b.BLOB)
    baseline_dir = Path(results['baseline_directory'])
    assert (baseline_dir / '.build-config').read_text() == results['config']
    assert all(hashlib.sha256((d.ROOT / p).read_bytes()).hexdigest() == digest
               for p,digest in results['headers'].items())
    for family, data in results['families'].items():
        folder = root / family
        base_path = folder / 'baseline/candidate.o'
        baseline_cell = data['cells']['baseline']
        frozen = baseline_dir / (data['source_path'].replace('/', '_') + '.o')
        assert baseline_cell['baseline_reproduced']
        assert hashlib.sha256(frozen.read_bytes()).hexdigest() == data['retained_hash']
        assert frozen.read_bytes() == (folder / 'retained.o').read_bytes() == base_path.read_bytes()
        baseline_command = shlex.split(baseline_cell['command'][-1])
        baseline_arguments = baseline_command[baseline_command.index('g++') + 1:]
        base = inspect(base_path)
        canonical = Object(base_path).canonical()
        family_report = {}
        for label, cell in data['cells'].items():
            path = folder / label / 'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            source = folder / label / Path(data['source_path']).name
            assert hashlib.sha256(source.read_bytes()).hexdigest() == cell['source_hash'] == baseline_cell['source_hash']
            command = shlex.split(cell['command'][-1])
            arguments = command[command.index('g++') + 1:]
            assert d.tc.REPRODUCE_BUGS in arguments
            assert cell['diagnostic_flags'] == list(FLAGS[label])
            assert all(arguments.count(f) == 1 for f in FLAGS[label])
            normalized = [s.replace('/' + family + '/' + label, '/' + family + '/baseline')
                          for s in arguments if s not in FLAGS[label]]
            assert normalized == baseline_arguments, (family, label, 'unexpected command change')
            assert '-O3' in arguments and '-frename-registers' in arguments
            log = (folder / label / 'compile.log').read_text()
            assert 'GNU C++ version 3.4.2  (Gentoo Linux 3.4.2-r2' in log
            assert 'GNU assembler version 2.15.92.0.2 (i386-pc-linux-gnu)' in log
            actual_functions = sorted(d.b.sizes(str(path)))
            actual_verdicts = {n: list(d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(path), n)))
                               for n in actual_functions if n in original_sizes}
            assert actual_functions == cell['functions'] and actual_verdicts == cell['verdicts']
            actual_globals = {line.split()[-1]: line.split()[-2] for line in
                subprocess.check_output(['nm', '-g', '--defined-only', str(path)], text=True).splitlines()}
            assert actual_globals == cell['globals']
            assert sorted(set(actual_functions) - set(baseline_cell['functions'])) == cell.get('added_functions', [])
            assert sorted(set(baseline_cell['functions']) - set(actual_functions)) == cell.get('missing_functions', [])
            actual_changed = [n for n in actual_functions if n in baseline_cell['functions'] and
                d.b.body(str(path), n) != d.b.body(str(base_path), n)]
            actual_gains = [n for n,v in actual_verdicts.items() if v[0] == 'EXACT' and
                           baseline_cell['verdicts'].get(n, ['MISSING'])[0] != 'EXACT']
            actual_losses = [n for n,v in baseline_cell['verdicts'].items() if v[0] == 'EXACT' and
                            actual_verdicts.get(n, ['MISSING'])[0] != 'EXACT']
            assert actual_changed == cell.get('changed_bodies', [])
            assert actual_gains == cell.get('gains', []) and actual_losses == cell.get('losses', [])
            current = inspect(path)
            cur_can = Object(path).canonical()
            record_changes = {name: [base['records'].get(name), row]
                              for name, row in current['records'].items()
                              if base['records'].get(name) != row}
            allocated_changes = {name: [base['allocated'].get(name), row]
                                 for name, row in current['allocated'].items()
                                 if base['allocated'].get(name) != row}
            relocation_changes = {name: {'before': [[str(k), n] for k,n in canonical[3].get(name, {}).items()],
                                         'after': [[str(k), n] for k,n in cur_can[3].get(name, {}).items()]}
                                  for name in set(canonical[3]) | set(cur_can[3])
                                  if canonical[3].get(name) != cur_can[3].get(name)}
            family_report[label] = {'functions': len(cell['functions']),
                'verdicts': cell['verdicts'], 'changed_bodies': cell.get('changed_bodies', []),
                'added_functions': cell.get('added_functions', []),
                'added_function_records': {n: current['records'][n] for n in cell.get('added_functions', [])},
                'added_function_sizes': {n: current['text_positions'][n][1] for n in cell.get('added_functions', [])},
                'missing_functions': cell.get('missing_functions', []),
                'defined_globals': len(cell['globals']),
                'added_globals': cell.get('added_globals', []), 'missing_globals': cell.get('missing_globals', []),
                'exact_gains': cell.get('gains', []), 'exact_losses': cell.get('losses', []),
                'records_changed': record_changes, 'allocated_noncode_changed': allocated_changes,
                'nobits_equal': base['nobits'] == current['nobits'],
                'noncode_relocations_equal': base['relocations'] == current['relocations'],
                'bindings_equal': canonical[2] == cur_can[2],
                'named_data_equal': named_data(base_path) == named_data(path),
                'noncode_relocations_before': base['relocations'],
                'noncode_relocations_after': current['relocations'],
                'relocation_consumer_changes': relocation_changes,
                'position_changes': {n: [base['text_positions'].get(n), p]
                                     for n,p in current['text_positions'].items()
                                     if base['text_positions'].get(n) != p},
                'target_edges': {n: {'original': edges(Path(d.b.BLOB), n),
                                     'candidate': edges(path, n)} for n in TARGETS[family]},
                'object_sha256': cell['object_hash']}
            family_report[label]['exact_gain_original_bytes'] = sum(original_sizes[n] for n in cell.get('gains', []))
            family_report[label]['exact_loss_original_bytes'] = sum(original_sizes[n] for n in cell.get('losses', []))
            graph = folder / label / (family + '.cpp.00.cgraph')
            signatures = {'V92ConvEncoder': ['int V92ConvolutionEncoder::inverseMap(int*)'],
                'V92EchoCanceller': ['void V92EchoCanceller::reset()', 'void V92EchoCanceller::setEchoDelay(unsigned int)'],
                'V90Phase4Modulator': ['void V90Phase4Modulator::recivedSUVtag()']}[family]
            if label in ('unit-off', 'both-off'):
                assert not graph.exists()
                family_report[label]['frontend_costs'] = None
            else:
                text = graph.read_text()
                family_report[label]['frontend_costs'] = {s: frontend_cost(text, s) for s in signatures}
                assert list(family_report[label]['frontend_costs'].values()) == {
                    'V92ConvEncoder': [98], 'V92EchoCanceller': [69, 18], 'V90Phase4Modulator': [82]}[family]
            print(family, label, 'changed', len(cell.get('changed_bodies', [])),
                  'gains', len(cell.get('gains', [])), 'losses', len(cell.get('losses', [])),
                  'target edge counts', [len(e['candidate']) for e in family_report[label]['target_edges'].values()], flush=True)
        report['families'][family] = family_report
    report['original_function_copy_verdicts'] = sum(len(r['verdicts']) for cs in report['families'].values() for r in cs.values())
    report['emitted_function_copies'] = sum(r['functions'] for cs in report['families'].values() for r in cs.values())
    validate(report)
    wrong = copy.deepcopy(report)
    wrong['families']['V92ConvEncoder']['auto-off']['target_edges'][TARGETS['V92ConvEncoder'][0]]['candidate'] = []
    try:
        validate(wrong)
    except AssertionError:
        pass
    else:
        raise AssertionError('omitted helper-edge negative did not fire')
    wrong = copy.deepcopy(report)
    wrong['families']['V92EchoCanceller']['auto-off']['added_functions'] = []
    try:
        validate(wrong)
    except AssertionError:
        pass
    else:
        raise AssertionError('omitted local inventory negative did not fire')
    try:
        frontend_cost('int absent(int*): 98 insns', 'int V92ConvolutionEncoder::inverseMap(int*)')
    except ValueError:
        pass
    else:
        raise AssertionError('missing frontend evidence negative did not fire')
    report['controls'] = {'raw_baselines': 3, 'missing_edge_rejected': True,
        'missing_local_inventory_rejected': True, 'missing_cost_evidence_rejected': True,
        'actual_object_source_header_hashes_verified': True, 'actual_12_commands_checked': True,
        'actual_304_body_verdicts_rescored': True}
    validate(json.loads(json.dumps(report)))
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('visibility cross: 12 valid cells; 3 frozen raw baselines; 304 original-body '
          'verdicts/312 emitted functions; 3 negatives; no profile adopted')


if __name__ == '__main__':
    d.REV, d.OUT_NAME = '6fe0b1e42', 'visibility-cross'
    d.SOURCE_PATHS = SOURCES
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-fdump-tree-original', '-fdump-tree-inlined')
    d.variants = lambda path, text: {'baseline': text}
    d.CELL_FLAGS = lambda path, label: FLAGS[label]
    if sys.argv[1:] == ['--audit']:
        audit()
    else:
        d.main()
        root = d.ROOT / 'build' / d.OUT_NAME
        result = json.loads((root / 'results.json').read_text())
        result['baseline_directory'] = str(Path(sys.argv[sys.argv.index('--baseline-dir') + 1]).resolve()
            if '--baseline-dir' in sys.argv else d.ROOT / 'build/tc_out')
        (root / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        matrix()
        audit()
