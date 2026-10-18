#!/usr/bin/env python3
"""Three retained-profile cells for a separately computed history span product.

Native DIV/IMUL product storage and the installed parser-fold trace motivate
one ordinary unsigned value lifetime. No flags, barriers or forced stores.
"""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from recovery_fax_debug_audit import Object
from recovery_generator_separation import named_data
from recovery_visibility_cross import edges

SOURCE = 'src/pump/v90/V92EchoCanceller.cpp'
PREFIX = '_ZN16V92EchoCanceller'
CTORS = [PREFIX + 'C%dEP13V92Parametersjj' % n for n in (1, 2)]
DUMPS = ('-v', '-save-temps', '-da', '-fdump-tree-original', '-fdump-tree-inlined')


def product(source):
    first, last, body = d.function(source, 'V92EchoCanceller::V92EchoCanceller')
    assert body.count('\tunsigned int span;') == 1
    old = '\thistoryAlloc = span + 2u * blockLen + span / blockLen * blockLen\n\t\t       + extra;'
    assert body.count(old) == 1
    changed = body.replace('\tunsigned int span;', '\tunsigned int span;\n\tunsigned int roundedSpan;')
    changed = changed.replace(old, '\troundedSpan = span / blockLen * blockLen;\n\thistoryAlloc = span + 2u * blockLen + roundedSpan + extra;')
    assert changed != body
    return source[:first] + changed + source[last:]


def variants(path, source):
    assert path == SOURCE
    separate = product(source)
    old = '\tspan = filterLengthMinusOne + echoDelay;'
    assert separate.count(old) == 1
    native_order = separate.replace(old, '\tspan = echoDelay + filterLengthMinusOne;')
    return {'baseline': source, 'product': separate, 'product-native-load-order': native_order}


def audit():
    root = d.ROOT / 'build' / d.OUT_NAME
    result = json.loads((root / 'results.json').read_text())
    baseline_dir = Path(result['baseline_directory'])
    manifest = json.loads((baseline_dir / 'manifest.json').read_text())
    assert manifest['revision'].startswith(d.REV)
    assert result['config'] == manifest['config'] == (baseline_dir / '.build-config').read_text()
    assert all(hashlib.sha256((d.ROOT / name).read_bytes()).hexdigest() == digest for name, digest in result['headers'].items())
    family = result['families']['V92EchoCanceller']
    folder = root / 'V92EchoCanceller'
    base_path = folder / 'baseline/candidate.o'
    assert base_path.read_bytes() == (baseline_dir / (SOURCE.replace('/', '_') + '.o')).read_bytes()
    assert hashlib.sha256(base_path.read_bytes()).hexdigest() == manifest['objects'][SOURCE.replace('/', '_') + '.o']
    old, metadata = Object(base_path).canonical(), inspect(base_path)
    original_sizes = d.b.sizes(d.b.BLOB)
    source = subprocess.check_output(['git', 'show', d.REV + ':' + SOURCE], cwd=d.ROOT, text=True)
    expected = variants(SOURCE, source)
    report = {'revision': manifest['revision'], 'cells': {}}
    assert set(family['cells']) == {'baseline', 'product', 'product-native-load-order'}
    for label, cell in family['cells'].items():
        path = folder / label / 'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        actual_source = (folder / label / 'V92EchoCanceller.cpp').read_text()
        assert actual_source == expected[label]
        assert hashlib.sha256(actual_source.encode()).hexdigest() == cell['source_hash']
        args = shlex.split(cell['command'][-1]); args = args[args.index('g++') + 1:]
        lines = result['config'].splitlines()
        flags = shlex.split(next(line[6:] for line in lines if line.startswith('flags ')))
        flags = ['-I/src/include' if word == '-Iinclude' else '/src/' + word
                 if word == 'tools/toolchain/period_compat.h' else word for word in flags]
        flags += list(DUMPS) + shlex.split(next(line[6:] for line in lines if line.startswith('cxx   ')))
        virtual = '/work/V92EchoCanceller/' + label
        assert args == ['-c'] + flags + [d.tc.REPRODUCE_BUGS, '-o', virtual + '/candidate.o', virtual + '/V92EchoCanceller.cpp']
        assert not cell['diagnostic_flags']
        log = (folder / label / 'compile.log').read_text()
        assert 'GNU C++ version 3.4.2  (Gentoo Linux 3.4.2-r2' in log
        assert 'GNU assembler version 2.15.92.0.2 (i386-pc-linux-gnu)' in log
        sizes = d.b.sizes(str(path))
        assert len(sizes) == 15 and sorted(sizes) == cell['functions']
        verdicts = {name: list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(path), name))) for name in sizes}
        assert verdicts == cell['verdicts']
        current, info = Object(path).canonical(), inspect(path)
        row = dict(verdicts=verdicts, sizes=sizes,
            gains=cell.get('gains', []), losses=cell.get('losses', []),
            raw_changed_bodies=cell.get('changed_bodies', []),
            normalized_code_changes=sorted(name for name in old[0] if old[0][name] != current[0][name]),
            bindings_equal=old[2] == current[2], named_data_equal=named_data(base_path) == named_data(path),
            record_changes={name: [metadata['records'].get(name), value] for name, value in info['records'].items() if metadata['records'].get(name) != value},
            noncode_changes={name: [metadata['allocated'].get(name), value] for name, value in info['allocated'].items() if metadata['allocated'].get(name) != value},
            nobits_equal=metadata['nobits'] == info['nobits'],
            noncode_relocations_equal=metadata['relocations'] == info['relocations'],
            position_changes={name: [metadata['text_positions'].get(name), value] for name, value in info['text_positions'].items() if metadata['text_positions'].get(name) != value},
            relocation_consumer_changes=sorted(name for name in set(old[3]) | set(current[3]) if old[3].get(name) != current[3].get(name)),
            ctor_edges={name: {'original': edges(Path(d.b.BLOB), name), 'candidate': edges(path, name)} for name in CTORS},
            total_original_bytes=sum(original_sizes[name] for name in sizes),
            exact_original_bytes=sum(original_sizes[name] for name, value in verdicts.items() if value[0] == 'EXACT'),
            object_sha256=cell['object_hash'])
        assert row['bindings_equal'] and row['named_data_equal'] and row['nobits_equal'] and row['noncode_relocations_equal']
        report['cells'][label] = row
        print(label, 'gains', row['gains'], 'losses', row['losses'], 'changed', row['normalized_code_changes'],
              'ctors', [verdicts[name] for name in CTORS], flush=True)
    baseline = report['cells']['baseline']
    exact_before = {name for name, verdict in baseline['verdicts'].items() if verdict[0] == 'EXACT'}
    assert len(exact_before) == 6 and baseline['exact_original_bytes'] == 729
    for label, row in report['cells'].items():
        assert row['losses'] == [] and row['total_original_bytes'] == 4309
        assert row['record_changes'] == row['position_changes'] == row['noncode_changes'] == {}
        assert row['relocation_consumer_changes'] == []
        assert row['normalized_code_changes'] == ([] if label == 'baseline' else CTORS)
        for ctor in CTORS:
            assert row['sizes'][ctor] == 313
            assert row['ctor_edges'][ctor]['original'] == row['ctor_edges'][ctor]['candidate']
            assert row['verdicts'][ctor] == (['BYTES', 112] if label == 'baseline' else
                                           ['BYTES', 2] if label == 'product' else ['EXACT', 0])
        assert row['gains'] == (CTORS if label == 'product-native-load-order' else [])
        assert row['exact_original_bytes'] == (1355 if label == 'product-native-load-order' else 729)
    # The product-only control deliberately retains the two native load-order
    # mismatches, so exactness must distinguish it from the selected source.
    assert report['cells']['product']['verdicts'][CTORS[0]] != report['cells']['product-native-load-order']['verdicts'][CTORS[0]]
    report['controls'] = dict(raw_baseline=True, product_only_not_misclassified_as_exact=True,
                            actual_45_body_verdicts_checked=True, complete_frozen_arguments_checked=True)
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    d.REV, d.OUT_NAME = 'c7cf4f8cd', 'echo-history-product'
    d.SOURCE_PATHS, d.DUMP_FLAGS, d.variants = (SOURCE,), DUMPS, variants
    if sys.argv[1:] == ['--audit']:
        audit()
    else:
        d.main()
        root = d.ROOT / 'build' / d.OUT_NAME
        result = json.loads((root / 'results.json').read_text())
        result['baseline_directory'] = str(Path(sys.argv[sys.argv.index('--baseline-dir') + 1]).resolve())
        (root / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        audit()
