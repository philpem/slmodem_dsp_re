#!/usr/bin/env python3
"""Two complete-TU reset-factoring cells at the unchanged retained profile.

Native reset regions correspond to four exported helpers. This tests ordinary
calls as a source-factoring hypothesis, without changing definitions, headers,
order or flags. Equivalent native regions do not prove original inline calls.
"""
import copy
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from recovery_fax_debug_audit import Object
from recovery_generator_separation import named_data
from recovery_visibility_cross import edges, frontend_cost
from elftools.elf.elffile import ELFFile

SOURCE = 'src/pump/v90/V92EchoCanceller.cpp'
PREFIX = '_ZN16V92EchoCanceller'
RESET = PREFIX + '5resetEv'
CTORS = [PREFIX + 'C%dEP13V92Parametersjj' % n for n in (1, 2)]
SIGNATURES = ['void V92EchoCanceller::reset()',
              'void V92EchoCanceller::zeroEchoCoeff()',
              'void V92EchoCanceller::resetEchoHistory()',
              'void V92EchoCanceller::setEchoBeta(float)',
              'void V92EchoCanceller::setDecayFactor(float)',
              'V92EchoCanceller::V92EchoCanceller(V92Parameters*, unsigned int, unsigned int)']


def validate(report):
    cells = report['cells']
    assert set(cells) == {'baseline', 'factored'}
    expected = {PREFIX + suffix for suffix in ('12setEchoDelayEj', '13zeroEchoCoeffEv',
                '16resetEchoHistoryEv', '5resetEv', 'D1Ev', 'D2Ev')}
    for label, row in cells.items():
        assert len(row['verdicts']) == 15 and row['bindings_equal']
        assert row['named_data_equal'] and row['nobits_equal'] and row['noncode_relocations_equal']
        assert {name for name, verdict in row['verdicts'].items() if verdict[0] == 'EXACT'} == expected
        assert row['gains'] == row['losses'] == []
        assert row['verdicts'][RESET] == ['EXACT', 0]
        assert row['exact_original_bytes'] == 729 and row['total_original_bytes'] == 4309
        assert row['normalized_code_changes'] == (CTORS if label == 'factored' else [])
        assert sorted(row['relocation_consumer_changes']) == (CTORS if label == 'factored' else [])
        for ctor in CTORS:
            edge = row['edges'][ctor]
            if label == 'factored':
                assert edge['candidate'] == edge['original']
                assert row['sizes'][ctor] == 313 and row['verdicts'][ctor] == ['BYTES', 112]
            else:
                assert edge['candidate'] != edge['original'] and row['sizes'][ctor] == 447
        cost = row['frontend_costs']['void V92EchoCanceller::reset()']
        assert cost == (58 if label == 'factored' else 69)
        block = row['inline_decisions']['void V92EchoCanceller::reset()']
        if label == 'factored':
            assert 'with 101 insns' in block and 'Inlining failed:' in block
        else:
            assert 'with 69 insns' in block and 'which now has 216 insns' in block


def factored(source):
    first, last, old = d.function(source, 'V92EchoCanceller::reset')
    assert 'echoCoeff[i] = 0.0f;' in old and 'echoHistory[i] = 0.0f;' in old
    assert old.count('state = V92_ECHO_FILTER_ONLY;') == 1
    new = old[:old.index('{')] + '''{
\tzeroEchoCoeff();
\tresetEchoHistory();
\tstate = V92_ECHO_FILTER_ONLY;
\tsetEchoBeta(0.0f);
\tsetDecayFactor(0.0f);
\tarma->reset();
}'''
    result = source[:first] + new + source[last:]
    assert source[:first] == result[:first] and source[last:] == result[first + len(new):]
    assert source != result
    return result


def variants(path, source):
    assert path == SOURCE
    return {'baseline': source, 'factored': factored(source)}


def compiler_proof():
    # Reuse the installed-compiler ELF observer, not guessed option defaults.
    settings = {name: getattr(d, name) for name in ('REV', 'OUT_NAME', 'SOURCE_PATHS', 'DUMP_FLAGS', 'variants', 'CELL_FLAGS')}
    from recovery_v92_inline_provenance import parameter_proof
    for name, value in settings.items():
        setattr(d, name, value)
    parameter_proof()


def audit():
    root = d.ROOT / 'build' / d.OUT_NAME
    result = json.loads((root / 'results.json').read_text())
    assert result['revision'] == d.REV
    baseline_dir = Path(result['baseline_directory'])
    manifest = json.loads((baseline_dir / 'manifest.json').read_text())
    assert manifest['revision'] == '7efb67f39a9f5ebe044494de1cd79c3d99d45552'
    assert result['config'] == manifest['config'] == (baseline_dir / '.build-config').read_text()
    assert all(hashlib.sha256((d.ROOT / path).read_bytes()).hexdigest() == expected
               for path, expected in result['headers'].items())
    family = result['families']['V92EchoCanceller']
    folder = root / 'V92EchoCanceller'
    base_path = folder / 'baseline/candidate.o'
    frozen = baseline_dir / (SOURCE.replace('/', '_') + '.o')
    assert frozen.read_bytes() == base_path.read_bytes() == (folder / 'retained.o').read_bytes()
    assert hashlib.sha256(frozen.read_bytes()).hexdigest() == manifest['objects'][frozen.name]
    base, old = inspect(base_path), Object(base_path).canonical()
    source = subprocess.check_output(['git', 'show', d.REV + ':' + SOURCE], cwd=d.ROOT, text=True)
    expected_sources = variants(SOURCE, source)
    commands, report = [], {'cells': {}, 'baseline_directory': str(baseline_dir)}
    for label, cell in family['cells'].items():
        path = folder / label / 'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        actual_source = (folder / label / 'V92EchoCanceller.cpp').read_text()
        assert actual_source == expected_sources[label]
        assert hashlib.sha256(actual_source.encode()).hexdigest() == cell['source_hash']
        command = shlex.split(cell['command'][-1])
        arguments = command[command.index('g++') + 1:]
        assert d.tc.REPRODUCE_BUGS in arguments and not cell['diagnostic_flags']
        config_lines = result['config'].splitlines()
        flags = shlex.split(next(line[6:] for line in config_lines if line.startswith('flags ')))
        flags = ['-I/src/include' if word == '-Iinclude' else
                 '/src/' + word if word == 'tools/toolchain/period_compat.h' else word for word in flags]
        flags += list(d.DUMP_FLAGS)
        flags += shlex.split(next(line[6:] for line in config_lines if line.startswith('cxx   ')))
        directory = '/work/V92EchoCanceller/' + label
        assert arguments == ['-c'] + flags + [d.tc.REPRODUCE_BUGS, '-o', directory + '/candidate.o',
                                             directory + '/V92EchoCanceller.cpp']
        commands.append([word.replace('/' + label + '/', '/baseline/') for word in arguments])
        log = (folder / label / 'compile.log').read_text()
        assert 'GNU C++ version 3.4.2  (Gentoo Linux 3.4.2-r2' in log
        assert 'GNU assembler version 2.15.92.0.2 (i386-pc-linux-gnu)' in log
        actual_functions = sorted(d.b.sizes(str(path)))
        actual_verdicts = {name: list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(path), name)))
                           for name in actual_functions}
        assert actual_functions == cell['functions'] and len(actual_functions) == 15
        assert actual_verdicts == cell['verdicts']
        current, canonical = inspect(path), Object(path).canonical()
        text = (folder / label / 'V92EchoCanceller.cpp.00.cgraph').read_text()
        decisions = {}
        for signature in SIGNATURES:
            blocks = ['Considering ' + block.split('\n\n', 1)[0]
                      for block in text.split('Considering ')[1:]
                      if block.startswith(signature + ' with ')]
            if blocks:
                assert len(blocks) == 1
                decisions[signature] = blocks[0]
        row = dict(verdicts=actual_verdicts, changed_bodies=cell.get('changed_bodies', []),
            gains=cell.get('gains', []), losses=cell.get('losses', []),
            edges={name: {'original': edges(Path(d.b.BLOB), name), 'candidate': edges(path, name)}
                   for name in [RESET] + CTORS},
            frontend_costs={signature: frontend_cost(text, signature) for signature in SIGNATURES},
            bindings_equal=old[2] == canonical[2], named_data_equal=named_data(base_path) == named_data(path),
            record_changes={name: [base['records'].get(name), row]
                            for name, row in current['records'].items() if base['records'].get(name) != row},
            noncode_changes={name: [base['allocated'].get(name), row]
                             for name, row in current['allocated'].items() if base['allocated'].get(name) != row},
            nobits_equal=base['nobits'] == current['nobits'],
            noncode_relocations_equal=base['relocations'] == current['relocations'],
            position_changes={name: [base['text_positions'].get(name), row]
                              for name, row in current['text_positions'].items() if base['text_positions'].get(name) != row},
            relocation_consumer_changes=[name for name in set(old[3]) | set(canonical[3])
                                         if old[3].get(name) != canonical[3].get(name)],
            object_sha256=cell['object_hash'], raw_equal_to_baseline=path.read_bytes() == base_path.read_bytes())
        row['inline_decisions'] = decisions
        row['normalized_code_changes'] = sorted(name for name in old[0] if old[0][name] != canonical[0][name])
        row['sizes'] = d.b.sizes(str(path))
        original_sizes = d.b.sizes(d.b.BLOB)
        row['total_original_bytes'] = sum(original_sizes[name] for name in actual_functions)
        row['exact_original_bytes'] = sum(original_sizes[name] for name, v in actual_verdicts.items() if v[0] == 'EXACT')
        row['emitted_function_bytes'] = sum(row['sizes'].values())
        assert row['bindings_equal'] and row['named_data_equal'] and row['nobits_equal']
        assert row['noncode_relocations_equal']
        report['cells'][label] = row
        print(label, 'changed', row['changed_bodies'], 'gains', row['gains'], 'losses', row['losses'],
              'costs', row['frontend_costs'], flush=True)
    assert commands[0] == commands[1], 'profile changed'
    report['original_function_verdicts'] = 30
    proof = json.loads((root / 'parameter-proof.json').read_text())
    assert hashlib.sha256((root / 'installed-cc1plus').read_bytes()).hexdigest() == proof['program_sha256']
    assert proof['records']['max-inline-insns-auto']['default'] == 100
    assert proof['records']['max-inline-insns-single']['default'] == 500
    report['installed_compiler_defaults'] = proof
    validate(report)
    wrong = copy.deepcopy(report)
    wrong['cells']['factored']['edges'][CTORS[0]]['candidate'] = []
    try:
        validate(wrong)
    except AssertionError:
        pass
    else:
        raise AssertionError('missing reset-edge control did not fire')
    try:
        frontend_cost('absent cost', 'void V92EchoCanceller::reset()')
    except ValueError:
        pass
    else:
        raise AssertionError('missing cost control did not fire')
    candidate = folder / 'factored/candidate.o'
    with candidate.open('rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        index = next(i for i, symbol in enumerate(table.iter_symbols()) if symbol.name == RESET)
        at = table['sh_offset'] + index * table['sh_entsize'] + 12
    mutant = bytearray(candidate.read_bytes())
    assert mutant[at] == 0x12  # GLOBAL FUNC; change binding alone to WEAK.
    mutant[at] = 0x22
    weak_path = root / 'weak-reset-export-control.o'
    weak_path.write_bytes(mutant)
    assert d.b.verdict(*d.b.body(d.b.BLOB, RESET), *d.b.body(str(weak_path), RESET)) == ('EXACT', 0)
    assert Object(weak_path).canonical()[2] != Object(candidate).canonical()[2]
    wrong = copy.deepcopy(report)
    wrong['cells']['factored']['bindings_equal'] = False
    try:
        validate(wrong)
    except AssertionError:
        pass
    else:
        raise AssertionError('weak export control did not fire')
    report['controls'] = dict(raw_baseline=True, changed_constructor_edges_observed=True,
        missing_edge_rejected=True, missing_cost_rejected=True, weak_export_rejected_despite_EXACT_body=True,
        actual_commands_hashes_and_30_body_verdicts_checked=True)
    validate(json.loads(json.dumps(report)))
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    d.REV, d.OUT_NAME = '7efb67f39', 'echo-reset-factoring'
    d.SOURCE_PATHS = (SOURCE,)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-fdump-tree-original', '-fdump-tree-inlined')
    d.variants = variants
    # Preserve the shared compiler helper while making its directory change
    # fail closed; the mutated command is the one recorded in results.json.
    real_run = subprocess.run
    def checked_run(command, *args, **kwargs):
        if isinstance(command, list) and command and command[0] == 'docker' and command[-1].startswith('cd '):
            command[-1] = command[-1].replace(' && ', ' || exit 1; ', 1)
        return real_run(command, *args, **kwargs)
    subprocess.run = checked_run
    if sys.argv[1:] == ['--compiler-proof']:
        compiler_proof()
    elif sys.argv[1:] == ['--audit']:
        audit()
    else:
        d.main()
        root = d.ROOT / 'build' / d.OUT_NAME
        result = json.loads((root / 'results.json').read_text())
        result['baseline_directory'] = str(Path(sys.argv[sys.argv.index('--baseline-dir') + 1]).resolve())
        (root / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        compiler_proof()
        audit()
