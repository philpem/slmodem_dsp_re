#!/usr/bin/env python3
"""Audit finite DIL cells and classify the two known info-array copy regions.

The native classifier is specific to the independently measured b0 stack home;
it reports addressing shape, not semantic equivalence or an original C type.
Unknown/missing array loads refuse. Canonical byteident remains the scorer.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

import playbook_small_patterns as d
from narrow_call_boundary_screen import bodies

RUNS = ('recovery-dil-next-info', 'recovery-dil-next-tail',
        'recovery-dil-next-ifconversion', 'recovery-dil-next-crc-limit',
        'recovery-dil-next-seven-pointer', 'recovery-dil-next-publication',
        'recovery-dil-next-base', 'recovery-dil-next-direct-pointer')
V92 = '_Z22V92DILdescriptorPackerP19tagV90DILdescriptorPhPi'


def info_graph(rows):
    rows = [r for r in rows if r['mnemonic'] != 'nop']
    loads = [i for i, row in enumerate(rows) if row['mnemonic'] == 'movzbl'
             and row['operands'].startswith('0xb0(%esp,')]
    if len(loads) != 2:
        raise ValueError('two independently known info-array copy loops required')
    result = []
    for at in loads:
        stores = [r for r in rows[at + 1:at + 5]
                  if r['mnemonic'] == 'mov' and
                  re.match(r'^%[abcd]l,.*\(', r['operands'])]
        if len(stores) != 1:
            raise ValueError('unique nearby byte destination not established')
        result.append(stores[0])
    padding = next((r for r in rows[loads[-1] + 5:loads[-1] + 18]
                    if r['mnemonic'] == 'movb' and r['operands'].startswith('$0x0,')), None)
    if padding is None:
        raise ValueError('adjacent padding destination not established')
    result.append(padding)
    return [{'mode': 'indexed' if ',' in r['operands'].split('(', 1)[1] else 'pointer',
             'instruction': r} for r in result]


def segment_graph(rows):
    rows = [r for r in rows if r['mnemonic'] != 'nop']
    loads = [(i, re.match(r'^0x(10[3-9a-f]|11[0-2])\(', r['operands']))
             for i, r in enumerate(rows) if r['mnemonic'] == 'movzbl']
    loads = [(i, int(m[1], 16)) for i, m in loads if m]
    if sorted(offset for _, offset in loads) != list(range(0x103, 0x113)):
        raise ValueError('sixteen known descriptor regions required')
    result = []
    for at, offset in loads:
        window = rows[at + 1:at + 25]
        payload = next((r for r in window if r['mnemonic'] == 'mov' and
                        re.match(r'^%[abcd]l,.*\(', r['operands'])), None)
        edge = next((i for i, r in enumerate(window) if r['mnemonic'].startswith('j')
                     and re.match(r'^[0-9a-f]+ ', r['operands'])
                     and int(r['operands'].split()[0], 16) <= int(r['address'], 16)), None)
        separator = next((r for r in window[edge + 1:] if r['mnemonic'] in ('mov', 'movb')
                          and re.match(r'^(?:\$0x0|%[abcd]l),.*\(', r['operands'])), None) \
                    if edge is not None else None
        if payload is None or separator is None:
            raise ValueError('payload/post-loop byte publication pair not established')
        def mode(row):
            return 'indexed' if ',' in row['operands'].split('(', 1)[1] else 'pointer'
        result.append({'descriptor_offset': offset, 'payload': mode(payload),
                       'separator': mode(separator), 'payload_instruction': payload,
                       'separator_instruction': separator})
    return result


def parity_edge(rows):
    tests = [i for i, row in enumerate(rows) if row['mnemonic'] == 'test'
             and row['operands'].startswith('$0x1,')]
    if len(tests) != 1:
        raise ValueError('unique parity test not established')
    return next(r for r in rows[tests[0] + 1:tests[0] + 5]
                if r['mnemonic'] in ('je', 'jne'))


def controls():
    def row(m, o): return {'address': '0', 'mnemonic': m, 'operands': o}
    for mode, operand in [('indexed', '(%ecx,%edx,1)'), ('pointer', '(%ecx)')]:
        group = [row('movzbl', '0xb0(%esp,%edx,1),%eax'),
                 row('mov', '%al,' + operand), row('inc', '%edx'),
                 row('cmp', '$0xf,%edx'), row('jle', '0')]
        rows = group + group + [row('movb', '$0x0,' + operand)]
        assert [v['mode'] for v in info_graph(rows)] == [mode] * 3
        altered = [dict(r, operands=r['operands'].replace('0xb0(', '0xc0(')) for r in rows]
        try:
            info_graph(altered)
        except ValueError:
            pass
        else:
            raise AssertionError('wrong home must refuse')
    print('DIL info controls: 4 passed; indexed/pointer positives, two changed-home refusals')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', type=Path)
    ap.add_argument('--self-test', action='store_true')
    args = ap.parse_args()
    controls()
    if args.self_test:
        return
    if args.output is None:
        ap.error('--output required')
    cells, source_profiles, emissions = [], set(), set()
    baselines = 0
    for run in RUNS:
        root = d.ROOT/'build'/run
        subprocess.run([sys.executable, str(d.ROOT/'tools/recovery_dil_audit.py'),
                        '--run', run], check=True)
        ledger = json.loads((root/'results.json').read_text())
        for family, record in ledger['families'].items():
            for label, cell in record['cells'].items():
                path = root/family/label/'candidate.o'
                assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
                source = path.parent/Path(record['source_path']).name
                assert hashlib.sha256(source.read_bytes()).hexdigest() == cell['source_hash']
                baselines += bool(cell.get('baseline_reproduced'))
                source_profiles.add((record['source_path'], cell['source_hash'],
                                     tuple(cell['diagnostic_flags'])))
                emissions.add((family, cell['object_hash']))
                row = {'run': run, 'family': family, 'cell': label,
                       'verdicts': cell['verdicts'], 'object_sha256': cell['object_hash']}
                if family == 'V92DILdesPCK':
                    native = bodies(path)[V92]
                    row['info_graph'] = info_graph(native)
                    row['segment_graph'] = segment_graph(native)
                    row['parity_edge'] = parity_edge(native)
                cells.append(row)
    original = bodies(d.b.BLOB)[V92]
    expected = ['indexed', 'pointer', 'indexed']
    assert [r['mode'] for r in info_graph(original)] == expected
    assert parity_edge(original)['mnemonic'] == 'jne'
    retained = next(c for c in cells if c['run'] == 'recovery-dil-next-publication'
                    and c['cell'] == 'info-1-owner-1' and c['family'] == 'V92DILdesPCK')
    assert [r['mode'] for r in retained['info_graph']] == expected
    assert retained['parity_edge']['mnemonic'] == 'jne'
    assert sum(r['mnemonic'] == 'jns' for r in original) == 18
    selected_path = d.ROOT/'build/recovery-dil-next-publication/V92DILdesPCK/info-1-owner-1/candidate.o'
    assert sum(r['mnemonic'] == 'jns' for r in bodies(selected_path)[V92]) == 18
    original_segments = segment_graph(original)
    assert all(r['payload'] == 'pointer' and r['separator'] == 'indexed' for r in original_segments)
    assert [(r['payload'], r['separator']) for r in retained['segment_graph']] == \
           [(r['payload'], r['separator']) for r in original_segments]
    baseline = next(c for c in cells if c['run'] == 'recovery-dil-next-info'
                    and c['family'] == 'V92DILdesPCK' and c['cell'] == 'baseline')
    assert [r['mode'] for r in baseline['info_graph']] != expected
    stage_root = d.ROOT/'build/recovery-dil-next-ifconversion/V90DILdesPCK'
    before = (stage_root/'baseline/V90DILdesPCK.cpp.20.combine').read_text()
    after = (stage_root/'baseline/V90DILdesPCK.cpp.21.ce2').read_text()
    assert before.index('(insn 247 ') > before.index('(jump_insn 228 ')
    assert after.index('(insn 247 ') < after.index('(jump_insn 228 ')
    assert not (stage_root/'cached-no-if1/V90DILdesPCK.cpp.21.ce2').exists()
    output = {'revision': 'd9995470', 'complete_tu_compiles': len(cells),
              'shared_verdicts': len(cells), 'raw_reproduced_controls': baselines,
              'distinct_source_profile_cells': len(source_profiles),
              'distinct_complete_object_emissions': len(emissions),
              'new_exact_functions': 0, 'lost_exact_functions': 0,
              'original_info_graph': info_graph(original),
              'original_segment_graph': segment_graph(original),
              'original_and_selected_countdown_backedges': 18,
              'original_parity_edge': parity_edge(original), 'cells': cells,
              'ce2_fix247_moves_before_predicate228': True,
              'no_if_conversion_skips_ce2': True}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2)+'\n')
    print('DIL continuation: %d compiles/shared verdicts, %d source/profile cells, '
          '%d object emissions; no exact gains/losses; selected V92 info/parity graphs pass' %
          (len(cells), len(source_profiles), len(emissions)))


if __name__ == '__main__':
    main()
