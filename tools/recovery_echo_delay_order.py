#!/usr/bin/env python3
"""Four-cell ordinary delay-definition order by unit-visibility cross.

The unit-off option is diagnostic. Moving one unchanged strong definition
tests visibility of delay setup separately from the later reset definition.
"""
import json
import copy
import hashlib
import shlex
import subprocess
import sys
from collections import Counter
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from recovery_visibility_cross import edges
from gcc3_value_carriers_audit import inspect
from recovery_fax_debug_audit import Object
from recovery_generator_separation import named_data

SOURCE = 'src/pump/v90/V92EchoCanceller.cpp'
PREFIX = '_ZN16V92EchoCanceller'
DELAY = PREFIX + '12setEchoDelayEj'
RESET = PREFIX + '5resetEv'
CTORS = [PREFIX + 'C%dEP13V92Parametersjj' % n for n in (1, 2)]
LABELS = ('baseline', 'current-unit-off', 'before', 'before-unit-off')


def validate(report):
    cells = report['cells']
    assert len(cells) == 4 and report['body_copy_verdicts'] == 60
    assert cells['baseline']['object_sha256'] == cells['before']['object_sha256']
    assert len({r['object_sha256'] for r in cells.values()}) == 3
    common_changes = sorted(CTORS + [RESET, PREFIX + '11setEchoBetaEf',
        PREFIX + '13setEchoParamsEffj', PREFIX + '14setDecayFactorEf',
        PREFIX + '8setStateE21V92EchoCancellerState'])
    for label, row in cells.items():
        assert row['functions'] == row['defined_globals'] == 15
        assert row['bindings_equal'] and row['named_data_equal'] and row['nobits_equal']
        assert row['noncode_relocations_equal'] and not row['record_changes']
        assert len(row['changed_bodies']) == dict(zip(LABELS, (0, 8, 0, 7)))[label]
        expected_changes = sorted(common_changes + ([DELAY] if label == 'current-unit-off' else [])) if label.endswith('unit-off') else []
        assert row['changed_bodies'] == expected_changes
        assert row['gains'] == []
        assert row['losses'] == ([DELAY] if label == 'current-unit-off' else [])
        assert set(row['noncode_changes']) == ({'.rodata.str1.4'} if label.endswith('unit-off') else set())
        for n, edge in row['ctor_edges'].items():
            original, actual = Counter(map(tuple, edge['original'])), Counter(map(tuple, edge['candidate']))
            assert original[('jmp', RESET)] == 1 and original[('call', DELAY)] == 0
            assert actual[('jmp', RESET)] == label.endswith('unit-off')
            assert actual[('call', DELAY)] == (2 if label == 'current-unit-off' else 0)
            if label == 'before-unit-off':
                assert actual == original
            assert row['verdicts'][n] == (['SIZE', 15] if label == 'current-unit-off' else
                ['BYTES', 112] if label == 'before-unit-off' else ['SIZE', 134])
        assert row['verdicts'][DELAY] == (['BYTES', 2] if label == 'current-unit-off' else ['EXACT', 0])


def moved(source):
    a,z,body = d.function(source, 'V92EchoCanceller::setEchoDelay')
    assert source[a-5:a] == 'void\n'
    block = source[a-5:z]
    remainder = source[:a-5] + source[z:]
    marker = '/*\n * The canceller\'s whole construction:'
    assert remainder.count(marker) == 1
    result = remainder.replace(marker, block + '\n\n' + marker)
    assert d.function(result, 'V92EchoCanceller::setEchoDelay')[2] == body
    assert result.index('V92EchoCanceller::setEchoDelay(') < result.index('V92EchoCanceller::V92EchoCanceller(')
    return result


def variants(path, source):
    before = moved(source)
    return {'baseline': source, 'current-unit-off': source,
            'before': before, 'before-unit-off': before}


def audit():
    root = d.ROOT / 'build' / d.OUT_NAME
    result = json.loads((root / 'results.json').read_text())
    family = result['families']['V92EchoCanceller']
    assert result['revision'] == d.REV
    frozen = Path(result['baseline_directory'])
    assert (frozen / '.build-config').read_text() == result['config']
    assert all(hashlib.sha256((d.ROOT / p).read_bytes()).hexdigest() == h for p,h in result['headers'].items())
    folder = root / 'V92EchoCanceller'
    base_path = folder / 'baseline/candidate.o'
    base = inspect(base_path)
    old = Object(base_path).canonical()
    frozen_object = frozen / (SOURCE.replace('/', '_') + '.o')
    assert hashlib.sha256(frozen_object.read_bytes()).hexdigest() == family['retained_hash']
    assert family['cells']['baseline']['baseline_reproduced']
    assert frozen_object.read_bytes() == base_path.read_bytes() == (folder / 'retained.o').read_bytes()
    baseline_cmd = shlex.split(family['cells']['baseline']['command'][-1])
    baseline_args = baseline_cmd[baseline_cmd.index('g++')+1:]
    source = subprocess.check_output(['git', 'show', d.REV + ':' + SOURCE], cwd=d.ROOT, text=True)
    with Path(d.b.BLOB).open('rb') as stream:
        symbols = list(ELFFile(stream).get_section_by_name('.symtab').iter_symbols())
        native = {s.name:[s['st_value'],s['st_size']] for s in symbols
                  if s['st_info']['type'] == 'STT_FUNC'}
        native_bindings = {s.name:[s['st_info']['type'],s['st_info']['bind'],s['st_other']['visibility']]
                           for s in symbols}
    names = sorted(d.b.sizes(str(base_path)))
    report = {'cells': {}, 'profile_adopted': False, 'definition_order_adopted': False,
        'native_text_positions': {n:native[n] for n in names}}
    for label, cell in family['cells'].items():
        path = folder / label / 'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        actual_source = (folder / label / Path(SOURCE).name).read_bytes()
        assert hashlib.sha256(actual_source).hexdigest() == cell['source_hash']
        assert actual_source.decode() == variants(SOURCE, source)[label]
        arguments = shlex.split(cell['command'][-1])
        arguments = arguments[arguments.index('g++')+1:]
        controls = ['-fno-unit-at-a-time'] if label.endswith('unit-off') else []
        assert cell['diagnostic_flags'] == controls and d.tc.REPRODUCE_BUGS in arguments
        assert all(arguments.count(f) == 1 for f in controls)
        assert [a.replace('/'+label+'/', '/baseline/') for a in arguments if a not in controls] == baseline_args
        log = (folder / label / 'compile.log').read_text()
        assert 'GNU C++ version 3.4.2  (Gentoo Linux 3.4.2-r2' in log
        assert 'GNU assembler version 2.15.92.0.2 (i386-pc-linux-gnu)' in log
        assert sorted(d.b.sizes(str(path))) == names == cell['functions']
        verdicts = {n:list(d.b.verdict(*d.b.body(d.b.BLOB,n), *d.b.body(str(path),n))) for n in names}
        assert verdicts == cell['verdicts']
        actual_globals = {line.split()[-1]:line.split()[-2] for line in subprocess.check_output(
            ['nm','-g','--defined-only',str(path)],text=True).splitlines()}
        assert actual_globals == cell['globals'] == family['cells']['baseline']['globals']
        changed = [n for n in names if d.b.body(str(path), n) != d.b.body(str(base_path), n)]
        assert changed == cell.get('changed_bodies', [])
        gains = [n for n in names if verdicts[n][0] == 'EXACT' and family['cells']['baseline']['verdicts'][n][0] != 'EXACT']
        losses = [n for n in names if verdicts[n][0] != 'EXACT' and family['cells']['baseline']['verdicts'][n][0] == 'EXACT']
        assert gains == cell.get('gains', []) and losses == cell.get('losses', [])
        current = inspect(path)
        assert all(native_bindings[n] == current['records'][n][:3] ==
                   ['STT_FUNC','STB_GLOBAL','STV_DEFAULT'] for n in names)
        canonical = Object(path).canonical()
        def strings(obj):
            with obj.open('rb') as stream:
                return Counter(ELFFile(stream).get_section_by_name('.rodata.str1.4').data().split(b'\0'))
        assert strings(path) == strings(base_path)
        row = {'functions':len(names), 'defined_globals':len(actual_globals),
            'verdicts': verdicts, 'changed_bodies': changed,
            'gains': cell.get('gains', []), 'losses': cell.get('losses', []),
            'ctor_edges': {n: {'original': edges(Path(d.b.BLOB), n), 'candidate': edges(path,n)} for n in CTORS},
            'bindings_equal': old[2] == canonical[2], 'named_data_equal': named_data(base_path) == named_data(path),
            'record_changes': {n:[base['records'].get(n),v] for n,v in current['records'].items() if base['records'].get(n)!=v},
            'noncode_changes': {n:[base['allocated'].get(n),v] for n,v in current['allocated'].items() if base['allocated'].get(n)!=v},
            'nobits_equal': base['nobits'] == current['nobits'],
            'noncode_relocations_equal': base['relocations'] == current['relocations'],
            'position_changes': {n:[base['text_positions'].get(n),v] for n,v in current['text_positions'].items() if base['text_positions'].get(n)!=v},
            'text_positions':current['text_positions'],
            'native_order_inversions':sum(1 for i,a in enumerate(names) for z in names[i+1:]
                if (native[a][0]-native[z][0]) * (current['text_positions'][a][0]-current['text_positions'][z][0]) < 0),
            'native_global_default_exports_verified':15,
            'string_multiset_equal':True,
            'relocation_consumer_changes': [n for n in set(old[3])|set(canonical[3]) if old[3].get(n)!=canonical[3].get(n)],
            'object_sha256': cell['object_hash']}
        report['cells'][label] = row
        print(label, 'changed', len(changed), 'gains',len(gains),'losses',len(losses), flush=True)
    report['body_copy_verdicts'] = sum(r['functions'] for r in report['cells'].values())
    validate(report)
    for name, mutate in (
        ('missing reset edge',lambda r:r['cells']['before-unit-off']['ctor_edges'][CTORS[0]].update(candidate=[])),
        ('hidden export change',lambda r:r['cells']['before']['record_changes'].update({DELAY:['GLOBAL','WEAK']})),
        ('false retained change',lambda r:r['cells']['before'].update(object_sha256='wrong'))):
        wrong = copy.deepcopy(report)
        mutate(wrong)
        try:
            validate(wrong)
        except AssertionError:
            pass
        else:
            raise AssertionError('negative did not fire: '+name)
    report['controls'] = {'raw_baselines':1,'actual_commands_checked':4,'actual_body_verdicts_rescored':60,
        'negative_controls_fired':3,'source_header_object_hashes_verified':True}
    (root / 'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('delay order: 4 valid cells, 60 actual body verdicts, 1 raw baseline, 3 negatives; no source order/profile adopted')


def production_control():
    root = d.ROOT / 'build' / d.OUT_NAME
    result = json.loads((root / 'results.json').read_text())
    cell = result['families']['V92EchoCanceller']['cells']['baseline']
    out = root / 'V92EchoCanceller/production-control'
    out.mkdir(exist_ok=True)
    (out / Path(SOURCE).name).write_bytes((d.ROOT / SOURCE).read_bytes())
    command = [s.replace('/V92EchoCanceller/baseline','/V92EchoCanceller/production-control') for s in cell['command']]
    with (out / 'compile.log').open('w') as log:
        code = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
    row = {'command':command,'exit':code,'source_sha256':hashlib.sha256((d.ROOT / SOURCE).read_bytes()).hexdigest()}
    assert code == 0
    row['object_sha256'] = hashlib.sha256((out / 'candidate.o').read_bytes()).hexdigest()
    assert (out / 'candidate.o').read_bytes() == (root / 'V92EchoCanceller/baseline/candidate.o').read_bytes()
    row['raw_equal'] = True
    (root / 'production-control.json').write_text(json.dumps(row,indent=2)+'\n')
    print('production comment control: raw identical',row['object_sha256'])


if __name__ == '__main__':
    d.REV, d.OUT_NAME = '7efb67f39', 'echo-delay-order'
    d.SOURCE_PATHS = (SOURCE,)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-fdump-tree-original', '-fdump-tree-inlined')
    d.variants = variants
    d.CELL_FLAGS = lambda path,label: ('-fno-unit-at-a-time',) if label.endswith('unit-off') else ()
    if sys.argv[1:] == ['--audit']:
        audit()
    elif sys.argv[1:] == ['--production-control']:
        production_control()
    else:
        d.main()
        root = d.ROOT / 'build' / d.OUT_NAME
        result = json.loads((root / 'results.json').read_text())
        result['baseline_directory'] = str(Path(sys.argv[sys.argv.index('--baseline-dir')+1]).resolve()
            if '--baseline-dir' in sys.argv else d.ROOT / 'build/tc_out')
        (root / 'results.json').write_text(json.dumps(result,indent=2)+'\n')
        audit()
