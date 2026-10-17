#!/usr/bin/env python3
"""Replay the four native-witnessed preinitdigital state/count positions.

The owner, reverse-clear and callback-publication families are closed here.
Only state_idx and descrambler.count positions cross; memset setup is separate.
"""
import itertools
import json
import re
import sys

import playbook_small_patterns as d
from recovery_preinit_publication import disassemble
from recovery_fax_debug_audit import Object
from recovery_fixedrc_apparatus import allocated
from recovery_generator_separation import named_data


def variants(path, source):
    if path.endswith('v34digital.c'):
        return {'baseline': source}
    a, z, fn = d.function(source, 'preinitdigital')
    result = {}
    for state, count in itertools.product((False, True), repeat=2):
        f = fn
        if state:
            assert f.count('\trx->state_idx = 0;\n') == 1
            f = f.replace('\trx->state_idx = 0;\n', '')
            f = f.replace('\t\ts->w[0] = 0;\n', '\t\ts->w[0] = 0;\n\t\trx->state_idx = 0;\n')
        if count:
            assert f.count('\t\tr->count = 0;\n') == 1
            f = f.replace('\t\tr->count = 0;\n', '')
            f = f.replace('\t\ts->nbits = 0x20;\n', '\t\ts->nbits = 0x20;\n\t\tr->count = 0;\n')
        label = 'baseline' if not state and not count else '%s-%s' % (
            'state-native' if state else 'state-late',
            'count-native' if count else 'count-late')
        result[label] = source[:a] + f + source[z:]
    assert len(result) == len(set(result.values())) == 4
    return result


def graph(text):
    tail = text[text.rfind('R_386_PC32 sysdep_memset'):]
    # Native owner assignments are independent prerequisites, already recovered.
    sm = re.search(r'lea\s+0x2a54\(%ebx\),(%e[a-z]+)', tail)
    dm = re.search(r'lea\s+0xe74\(%ebx\),(%e[a-z]+)', tail)
    def between(needle, left, right):
        p, l, r = tail.find(needle), tail.find(left), tail.find(right)
        return min(p, l, r) >= 0 and l < p < r
    return {
        'state_between_scrambler_w0_and_nbits': bool(sm) and between(
            '0x144c(%ebx)', '0x2a54(%ebx)\n', '0x10(%s)' % sm[1]),
        'count_between_nbits_and_w3': bool(sm and dm) and between(
            '0xc(%s)' % dm[1], '0x10(%s)' % sm[1], '0xc(%s)' % sm[1]),
    }


def audit():
    root = d.ROOT / 'build' / d.OUT_NAME
    base = root / 'v34shell/baseline/candidate.o'
    old = Object(base).canonical()
    results = json.loads((root / 'results.json').read_text())
    digital = root / 'v34digital/baseline/candidate.o'
    report = {'source_cells': 4,
              'pair_bodies': len(old[0]) + len(Object(digital).canonical()[0]),
              'cells': {}}
    assert report['pair_bodies'] == 13
    for family in ('v34shell', 'v34digital'):
        assert (root / family / 'baseline/candidate.o').read_bytes() == (root / family / 'retained.o').read_bytes()
    for label, data in results['families']['v34shell']['cells'].items():
        candidate = root / 'v34shell' / label / 'candidate.o'
        cur = Object(candidate).canonical()
        assert old[1:] == cur[1:], (label, 'data/binding/consumer drift')
        assert named_data(base) == named_data(candidate)
        assert {n: v for n, v in allocated(base).items() if n != '.text'} == {
            n: v for n, v in allocated(candidate).items() if n != '.text'}
        changed = [n for n in old[0] if old[0][n] != cur[0][n]]
        assert changed in ([], ['preinitdigital']), (label, changed)
        report['cells'][label] = {'changed_bodies': changed,
            'verdict': data['verdicts']['preinitdigital'],
            'gains': data.get('gains', []), 'losses': data.get('losses', []),
            'graph': graph(disassemble(candidate))}
        native = label.startswith('state-native')
        assert data['verdicts']['preinitdigital'] == ['BYTES', 106 if native else 147]
        assert not data.get('gains', []) and not data.get('losses', [])
        assert all(v == native for v in report['cells'][label]['graph'].values())
        assert changed == (['preinitdigital'] if native else [])
    original = disassemble(d.b.BLOB)
    (root / 'reference.dis').write_text(original)
    report['reference_graph'] = graph(original)
    assert all(report['reference_graph'].values()), report
    report['baseline_raw_controls'] = 2
    report['valid_compiles'] = 5
    report['candidate_pair_body_verdicts'] = 39
    report['unchanged_neighbour_verdicts'] = 36
    report['defined_globals'] = len(results['families']['v34shell']['cells']['baseline']['globals'])
    report['named_data_records'] = len(named_data(base))
    report['external_imports'] = sorted(s.name for s in Object(base).symbols
        if s['st_info']['bind'] != 'STB_LOCAL' and s['st_shndx'] == 'SHN_UNDEF')
    report['exact_names'] = sorted(n for n, v in results['families']['v34shell']['cells']['baseline']['verdicts'].items() if v[0] == 'EXACT')
    assert report['exact_names'] == ['preinitV34', 'scaleVector', 'setScramble']
    assert report['defined_globals'] == 29 and report['named_data_records'] == 16
    assert len(report['external_imports']) == 10
    # Demonstrate the native detector rejects a changed state address.
    assert not graph(original.replace('0x144c(%ebx)', '0x1450(%ebx)'))['state_between_scrambler_w0_and_nbits']
    assert not graph(original.replace('0xc(%ecx)', '0xe(%ecx)'))['count_between_nbits_and_w3']
    assert not any(report['cells']['baseline']['graph'].values())
    assert all(report['cells']['state-native-count-late']['graph'].values())
    objects = {label: (root / 'v34shell' / label / 'candidate.o').read_bytes()
               for label in report['cells']}
    assert objects['baseline'] == objects['state-late-count-native']
    assert objects['state-native-count-late'] == objects['state-native-count-native']
    report['distinct_shell_object_emissions'] = len(set(objects.values()))
    report['selected'] = 'state-native-count-late'
    report['controls'] = ['reference-positive', 'baseline-negative',
                          'wrong-state-address-negative', 'wrong-count-address-negative',
                          'count-only-raw-identical', 'combined-raw-identical-to-state-only']
    report['stage_order'] = stage_audit(root)
    (root / 'interleave-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    for label, row in report['cells'].items():
        print(label, row)


def stage_audit(root):
    """Locate the compare motion and its hard-register/flags dependencies."""
    cell = root / 'v34shell/state-native-count-late'
    expected = {
        '01.rtl': [239, 241, 244, 246, 253, 254],
        '20.combine': [239, 241, 244, 246, 254],
        '24.lreg': [239, 241, 244, 246, 254],
        '25.greg': [239, 241, 244, 246, 254],
        '28.peephole2': [239, 241, 415, 416, 413, 414, 254],
        '30.rnreg': [239, 241, 415, 416, 413, 414, 254],
        '31.bbro': [239, 241, 415, 416, 413, 414, 254],
        '33.sched2': [415, 239, 241, 413, 254, 416, 414],
        '35.mach': [415, 239, 241, 413, 254, 416, 414],
    }
    report = {}
    for stage, ids in expected.items():
        text = (cell / ('v34shell.c.' + stage)).read_text()
        text = text.split(';; Function preinitdigital', 1)[1].split(';; Function ', 1)[0]
        blocks = {int(m[1]): m[0] for m in re.finditer(
            r'^\((?:insn|jump_insn)(?::\w+)? (\d+)[\s\S]*?(?=^\(|\Z)', text, re.M)}
        relevant = set((239, 241, 244, 246, 253, 254, 413, 414, 415, 416))
        order = [n for n in blocks if n in relevant]
        assert order == ids, (stage, order)
        report[stage] = {'ordered_ids': order}
        if stage == '24.lreg':
            assert 'reg/v/f:SI 83 [ r ]' in blocks[239]
        if stage in ('25.greg', '28.peephole2', '30.rnreg', '31.bbro', '33.sched2', '35.mach'):
            assert '1 dx [orig:83 r ]' in blocks[239]
        if stage == '33.sched2':
            assert 'REG_DEP_ANTI 239' in blocks[413] and 'REG_DEP_ANTI 241' in blocks[413]
            assert 'clobber (reg:CC 17 flags)' in blocks[413]
            assert 'REG_DEP_OUTPUT 413' in blocks[254]
    report['conclusion'] = ('Compare 254 crosses data-enable/train stores only at sched2. '
        'It remains after descrambler clears239/241 throughout. greg assigns r83 to EDX; '
        'peephole2 introduces XOR EDX413, anti-dependent on both r uses and clobbering flags; '
        'compare254 has an output dependency on413. Original ECX owner avoids that EDX chain. '
        'This is a register-allocation/peephole/scheduling boundary, not evidence for moving the source role test.')
    return report


def production_control():
    import subprocess
    root = d.ROOT / 'build' / d.OUT_NAME
    results = json.loads((root / 'results.json').read_text())
    selected = 'state-native-count-late'
    out = root / 'v34shell/production-control'
    out.mkdir(exist_ok=True)
    (out / 'v34shell.c').write_bytes((d.ROOT / 'src/pump/v34/v34shell.c').read_bytes())
    command = [s.replace('/v34shell/' + selected, '/v34shell/production-control')
               for s in results['families']['v34shell']['cells'][selected]['command']]
    with (out / 'compile.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    assert (out / 'candidate.o').read_bytes() == (root / 'v34shell' / selected / 'candidate.o').read_bytes()
    (out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    print('production control: raw-identical complete selected TU')


if __name__ == '__main__':
    d.REV = '4a6519cba4baa7e9edf25831b9cf9c967a14ec6e'
    d.OUT_NAME = 'preinit-interleave'
    d.SOURCE_PATHS = ('src/pump/v34/v34shell.c', 'src/pump/v34/v34digital.c')
    d.DUMP_FLAGS = ('-da', '-v', '-save-temps')
    d.variants = variants
    if sys.argv[1:] == ['--production-control']:
        production_control()
    elif sys.argv[1:] == ['--audit']:
        audit()
    else:
        d.main()
        audit()
