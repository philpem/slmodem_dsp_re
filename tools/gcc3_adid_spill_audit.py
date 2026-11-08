#!/usr/bin/env python3
"""Audit complete ADID controls, named reload spills, context and production."""
import hashlib
import json
import re
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_phase_grouping_audit import TARGET

FAMILY = 'V90AutoDigitalImpDetector'
ROOTS = ('gcc3-adid-spill-context', 'gcc3-adid-group-state')
UREF = '_ZN25V90AutoDigitalImpDetector10updateUrefEv'


def masked(path, name):
    body, relocs = d.b.body(str(path), name)
    data = bytearray(body)
    for offset in relocs:
        data[offset:offset + 4] = b'\0' * 4
    return bytes(data), relocs


def named_spills(folder, trace):
    rtl = (folder / (FAMILY + '.cpp.01.rtl')).read_text()
    fn = rtl.split(';; Function void V90AutoDigitalImpDetector::uniteLinMappInfoOfUnsuspectedPhases(unsigned char)', 1)[1].split(';; Function ', 1)[0]
    pseudos = {name: int(number) for number, name in re.findall(r'reg/v:SI (\d+) \[ (count|best|bestGroup) \]', fn)}
    assert set(pseudos) == {'count', 'best', 'bestGroup'}
    frame = trace['frames'][-1]['layout']
    displacement = frame['stack_pointer_offset'] - frame['frame_pointer_offset']
    slots = {}
    for event in trace['events']:
        for caller in event['caller_values']:
            if caller['function'] != 'alter_reg':
                continue
            number = caller['available_values']['i']
            node = event['returned']['operands'][0]
            assert node['code'] == 'PLUS' and node['operands'][0]['regno'] == 20
            offset = node['operands'][1]['value']
            for name, pseudo in pseudos.items():
                if pseudo == number:
                    slots[name] = {'pseudo': pseudo, 'frame_offset': offset,
                                   'stack_pointer_offset': displacement + offset}
    assert set(slots) == set(pseudos)
    return slots


def main():
    report = {}
    total = verdicts = 0
    for rootname in ROOTS:
        root = d.ROOT / 'build' / rootname
        cells = json.loads((root / 'results.json').read_text())['families'][FAMILY]['cells']
        basepath = root / FAMILY / 'baseline/candidate.o'
        base = inspect(basepath)
        rows = {}
        for label, cell in cells.items():
            path = root / FAMILY / label / 'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            current = inspect(path)
            assert current == base, (rootname, label, 'metadata/named data')
            datachanges = [section for section, contents in allocated(path).items()
                           if allocated(basepath).get(section) != contents]
            assert not datachanges or label == 'closest-context'
            assert len(cell['verdicts']) == 34 and not cell.get('losses')
            assert cell.get('gains', []) == ([TARGET] if label == 'closest-phase-state' else [])
            if label != 'closest-context':
                assert set(cell.get('changed_bodies', [])) <= {TARGET}
            rows[label] = {'target_verdict': cell['verdicts'][TARGET],
                           'allocated_nontext_changes': datachanges,
                           'raw_body_changes': cell.get('changed_bodies', []),
                           'common_body_verdicts': 34}
            total += 1
            verdicts += 34
        report[rootname] = rows
    assert (total, verdicts) == (9, 306)
    context = d.ROOT / 'build' / ROOTS[0] / FAMILY
    close, moved = context / 'closest/candidate.o', context / 'closest-context/candidate.o'
    assert all(masked(close, n) == masked(moved, n) for n in d.b.sizes(str(close)))
    # All context raw-body differences are relocation addends, not instruction changes.
    assert len(report[ROOTS[0]]['closest-context']['raw_body_changes']) == 5
    traces = {}
    events = 0
    for source, observation in ((ROOTS[0], 'gcc3-adid-spill-observation'),
                                (ROOTS[1], 'gcc3-adid-group-state-observation')):
        obs = json.loads((d.ROOT / 'build' / observation / 'results.json').read_text())
        rows = {}
        for label, entry in obs['cells'].items():
            trace = entry['trace']
            assert entry['raw_object_unchanged'] and trace['exit_codes'] == [0]
            assert not trace['errors'] and not trace['pending']
            assert len(trace['events']) == 13
            assert sum(bool(e['caller_values']) for e in trace['events']) == 8
            rows[label] = named_spills(d.ROOT / 'build' / source / FAMILY / label, trace)
            events += 13
        traces[source] = rows
    assert traces[ROOTS[0]]['closest'] == traces[ROOTS[0]]['closest-context']
    winner = traces[ROOTS[1]]['closest-phase-state']
    assert winner['count']['stack_pointer_offset'] == 0x24
    assert winner['best']['stack_pointer_offset'] == 0x20
    assert traces[ROOTS[1]]['closest']['count']['stack_pointer_offset'] == 0x20
    assert traces[ROOTS[1]]['closest']['best']['stack_pointer_offset'] == 0x24
    positive = json.loads((d.ROOT / 'build/gcc3-adid-spill-positive/results.json').read_text())
    assert len(positive['cells']) == 5
    for label, entry in positive['cells'].items():
        assert entry['raw_object_unchanged'] and len(entry['trace']['events']) == 4
        assert entry['trace']['exit_codes'] == [0] and not entry['trace']['errors']
        path = d.ROOT / 'build' / ROOTS[1] / FAMILY / label / 'candidate.o'
        assert d.b.verdict(*d.b.body(d.b.BLOB, UREF), *d.b.body(str(path), UREF))[0] == 'EXACT'
        events += 4
    winnerpath = d.ROOT / 'build' / ROOTS[1] / FAMILY / 'closest-phase-state/candidate.o'
    production = d.ROOT / 'build/tc_out/src_pump_v90_V90AutoDigitalImpDetector.cpp.o'
    assert production.read_bytes() == winnerpath.read_bytes()
    source = (d.ROOT / 'src/pump/v90/V90AutoDigitalImpDetector.cpp').read_text()
    _, _, grouping = d.function(source, 'V90AutoDigitalImpDetector::uniteLinMappInfoOfUnsuspectedPhases')
    _, _, uref = d.function(source, 'V90AutoDigitalImpDetector::unitePhasesInfoOfUref')
    report.update({'named_spills': traces, 'source_cells': total, 'common_body_verdicts': verdicts,
                   'observation_runs': 14, 'target_and_positive_allocations': events,
                   'production_raw_equal_to_winner': True})
    assert events == 137
    (d.ROOT / 'build/adid-spill-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('9 cells /306 verdicts; 14 raw-equal observations /137 allocations; production raw equals winner')


if __name__ == '__main__':
    main()
