#!/usr/bin/env python3
"""Replay the bounded SIZE spill observations and audit complete translation units."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks

FAMILIES = {
    'V90MappingParamsInt': ('mapping', 'getConstellationsIndex',
        'unsigned int getConstellationsIndex(V90MappingParams*, int*)',
        'src/pump/v90/V90MappingParamsInt.cpp', 'i-first'),
    'psd': ('psd', '_ZN3Psd7processEPfjS0_NS_12OutputOptionE',
        'void Psd::process(float*, unsigned int, float*, Psd::OutputOption)',
        'src/dsp/psd.cpp', 'state-before-division'),
}


def homes(folder, family, header, trace):
    stream = chunks(folder / (family + '.cpp.01.rtl'), header)
    assert len(stream) == 1
    names = ('count', 'i') if family != 'psd' else ('frame', 'pos')
    pseudos = {name: int(number) for number, name in re.findall(
        r'reg/v[^ ]* (\d+) \[ (' + '|'.join(names) + r') \]', stream[0])}
    assert set(pseudos) == set(names)
    if family == 'psd':
        quotient = re.findall(r'\(set \(reg:SI (\d+)\)\s+\(udiv:SI', stream[0])
        assert len(quotient) == 1
        pseudos['frames'] = int(quotient[0])
        # The named frame-count variable receives this division result.
        assert re.search(r'\(set \(reg/v:SI \d+ \[ frames \]\)\s+\(reg:SI ' +
                         quotient[0] + r'\)', stream[0])
    layout = trace['frames'][-1]['layout']
    displacement = layout['stack_pointer_offset'] - layout['frame_pointer_offset']
    result = {}
    for event in trace['events']:
        for caller in event['caller_values']:
            if caller['function'] != 'alter_reg':
                continue
            number = caller['available_values']['i']
            for name, pseudo in pseudos.items():
                if number != pseudo:
                    continue
                node = event['returned']['operands'][0]
                assert node['code'] == 'PLUS' and node['operands'][0]['regno'] == 20
                offset = node['operands'][1]['value']
                assert name not in result
                result[name] = {'pseudo': pseudo, 'frame_offset': offset,
                                'stack_offset': displacement + offset}
    assert set(result) == set(pseudos)
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--integrated', action='store_true')
    ap.add_argument('--psd-dispatch', action='store_true', help='verify the subsequent switch integration')
    args = ap.parse_args()
    root = d.ROOT / 'build/gcc3-size-spill-lifetime'
    ledger = json.loads((root / 'results.json').read_text())
    report = {}
    verdict_count = request_count = 0
    for family, (observation, target, header, source, winner) in FAMILIES.items():
        obs = json.loads((d.ROOT / ('build/gcc3-size-spill-' + observation +
                         '-lifetime-observation/results.json')).read_text())
        cells = ledger['families'][family]['cells']
        base = root / family / 'baseline/candidate.o'
        rows = {}
        for label, cell in cells.items():
            folder = root / family / label
            path = folder / 'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            assert inspect(path) == inspect(base) and allocated(path) == allocated(base)
            changed = [n for n in cell['functions'] if masked(path, n) != masked(base, n)]
            expected = ([target, 'setV92CPpckFromParamsInfo'] if family != 'psd'
                        else [target]) if label == winner else []
            assert changed == expected
            for name in changed:
                permutation = ({4: 8, 8: 4} if name == target else
                               {0x10: 0x14, 0x14: 0x10}) if family != 'psd' else {
                               0x18: 0x1c, 0x1c: 0x20, 0x20: 0x18}
                def exchange(match):
                    offset = int(match[1], 16)
                    return hex(permutation.get(offset, offset)) + '(%esp)'
                before = d.b.insns(str(base), name)
                after = d.b.insns(str(path), name)
                assert [(m, re.sub(r'(0x[0-9a-f]+)\(%esp\)', exchange, o))
                        for m, o in before] == after, (family, name, 'non-home change')
            grades = {n: d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(path), n))
                      for n in cell['verdicts']}
            assert {n: list(v) for n, v in grades.items()} == cell['verdicts']
            assert grades == {n: tuple(v) for n, v in cells['baseline']['verdicts'].items()}
            entry = obs['cells'][label]
            trace = entry['trace']
            assert entry['raw_object_unchanged'] and trace['exit_codes'] == [0]
            assert not trace['errors'] and not trace['pending']
            assert len(trace['events']) == (5 if family != 'psd' else 9)
            slots = homes(folder, family, header, trace)
            offsets = {n: v['stack_offset'] for n, v in slots.items()}
            expected_slots = ({'count': 4, 'i': 8} if label == winner else
                              {'count': 8, 'i': 4}) if family != 'psd' else (
                              {'frames': 0x18, 'pos': 0x1c, 'frame': 0x20} if label == winner
                              else {'frames': 0x20, 'pos': 0x18, 'frame': 0x1c})
            assert offsets == expected_slots, (family, label, offsets)
            rows[label] = {'homes': slots, 'changed_bodies': changed, 'verdicts': grades,
                           'metadata_data_unchanged': True, 'raw_observation_equal': True}
            verdict_count += len(grades)
            request_count += len(trace['events'])
        if args.integrated:
            folder = root / family / winner
            if family == 'psd' and args.psd_dispatch:
                folder = d.ROOT / 'build/gcc3-psd-dispatch/psd/switch'
            assert (d.ROOT / source).read_bytes() == (folder / (family + '.cpp')).read_bytes()
            assert (d.ROOT / ('build/tc_out/' + source.replace('/', '_') + '.o')).read_bytes() == (folder / 'candidate.o').read_bytes()
        report[family] = rows
    assert (verdict_count, request_count) == (72, 42)
    report['denominator'] = {'source_cells': 6, 'body_verdicts': verdict_count,
                             'raw_equal_observations': 6, 'stack_requests': request_count}
    report['integrated'] = args.integrated
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('6 cells /72 body verdicts; 6 raw-equal observations /42 requests; both original home maps recovered; zero exact gains/losses')


if __name__ == '__main__':
    main()
