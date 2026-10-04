#!/usr/bin/env python3
"""Audit the bounded Uref source cross and replay observed GCC frame arithmetic."""
import copy
import json
from collections import Counter
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect, UREF, STUDY, ADID

UNITE = ADID+'21unitePhasesInfoOfUrefEs'
SECOND = ADID+'18porcessSecondStudyEv'

def masked_body(path, name):
    body, relocations = d.b.body(path, name)
    body = bytearray(body)
    for offset in relocations:
        body[offset:offset+4] = b'\0'*4
    return bytes(body), relocations

def validate_trace(trace, constant):
    assert trace['exit_codes'] == [0] and not trace['errors'] and not trace['pending']
    slots = [(e['mode'], e['size'], e['frame_before'], e['frame_after']) for e in trace['events']]
    assert slots == [('HImode', 2, 0, -2), ('HImode', 2, -2, -4),
                     ('QImode', 1, -4, -5), ('BLKmode', 0, -5, -8)]
    assert all('returned' in e for e in trace['events'])
    known = [e['known_incoming_boundary'] for e in trace['callees']
             if e['callee'] == 'unitePhasesInfoOfUref' and e['asm_written']]
    assert known == [32 if constant else 128]*2
    assert len(trace['frames']) == 15
    assert trace['frames'][-1]['reload_completed']
    for event in trace['frames']:
        assert not event['frame_pointer_needed']
        assert event['locals_size'] == event['outgoing_args_size'] == 8
        assert event['stack_alignment_needed'] == 32
        assert event['preferred_stack_boundary'] == (32 if constant else 128)
        layout = event['layout']
        assert layout['nregs'] == 2 and layout['va_arg_size'] == 0
        # GCC 3.4.2 i386.c: return address, pushed registers, local alignment,
        # local bytes, outgoing arguments, then the known callee's alignment.
        offset = 4 + 4*layout['nregs']
        align = event['stack_alignment_needed']//8
        padding1 = (-offset) % align
        fp_offset = offset + padding1
        offset = fp_offset + event['locals_size'] + event['outgoing_args_size']
        padding2 = (-offset) % (event['preferred_stack_boundary']//8)
        assert layout['padding1'] == padding1 and layout['padding2'] == padding2
        assert layout['frame_pointer_offset'] == fp_offset
        assert layout['stack_pointer_offset'] == offset + padding2
        assert layout['outgoing_arguments_size'] == event['outgoing_args_size']
        assert layout['to_allocate'] == event['locals_size'] + event['outgoing_args_size'] + padding1 + padding2

def main():
    reports = []
    for package in ['stack-cross', 'nan-second']:
        root = d.ROOT/'build'/('gcc3-uref-'+package)
        cells = json.loads((root/'results.json').read_text())['families']['V90AutoDigitalImpDetector']['cells']
        assert len(cells) == 4 and cells['baseline']['baseline_reproduced']
        baseline = root/'V90AutoDigitalImpDetector/baseline/candidate.o'
        base = inspect(baseline)
        for label, cell in cells.items():
            first = label.startswith('constant-1') or label.startswith('predecessor-1')
            rounding = label.endswith('rounding-1') or label.startswith('predecessor-1')
            second = label.endswith('second-1')
            path = root/'V90AutoDigitalImpDetector'/label/'candidate.o'
            got = inspect(path)
            records = dict(base['records'])
            if first and second:
                records.pop('nanf')
            assert got['records'] == records and got['nobits'] == base['nobits']
            assert got['allocated'].keys() == base['allocated'].keys()
            for name, hexdata in got['allocated'].items():
                before, after = bytes.fromhex(base['allocated'][name]), bytes.fromhex(hexdata)
                if name == '.rodata.cst4':
                    chunks = lambda buf: Counter(buf[i:i+4] for i in range(0, len(buf), 4))
                    expected = chunks(before)
                    expected[b'\0\0\xc0\x7f'] += 2*first + second
                    assert chunks(after) == expected
                elif name == '.rodata.str1.1':
                    expected = Counter(before.split(b'\0'))
                    if first and second:
                        expected[b''] -= 1
                    assert Counter(after.split(b'\0')) == expected
                else:
                    assert before == after, (package, label, name)
            assert len(got['relocations']) == len(base['relocations'])
            changes = []
            for old, new in zip(base['relocations'], got['relocations']):
                assert old[:3] == new[:3]
                if old != new:
                    assert rounding and old[3][:2] == new[3][:2] == ('audited-code-destination', STUDY)
                    changes.append([old, new])
            expected = ([UREF] if first or rounding else []) + ([UNITE] if first else [])
            expected += ([STUDY] if rounding else []) + ([SECOND] if second else [])
            changed = [name for name in cell['functions'] if masked_body(baseline, name) != masked_body(path, name)]
            assert sorted(changed) == sorted(expected), (package, label, changed)
            assert cell.get('gains', []) == ([UREF] if first and rounding else [])
            assert not cell.get('losses', [])
            reports.append({'package': package, 'cell': label, 'body_verdicts': len(cell['verdicts']),
                            'actual_changed_bodies': changed, 'relocated_code_destinations': changes,
                            'quiet_nan_entries_added': 2*first+second, 'nanf_import_removed': first and second,
                            'raw_changed_bodies': cell.get('changed_bodies', []),
                            'text_positions': got['text_positions']})
    observations = json.loads((d.ROOT/'build/gcc3-uref-stack-observation/results-gcc3-uref-stack-cross.json').read_text())
    assert len(observations['cells']) == 4
    for label, cell in observations['cells'].items():
        assert cell['raw_object_unchanged']
        validate_trace(cell['trace'], label.startswith('constant-1'))
    trace = observations['cells']['baseline']['trace']
    refusals = []
    for fault in ['missing_target', 'wrong_frame', 'wrong_boundary']:
        bad = copy.deepcopy(trace)
        if fault == 'missing_target': bad['events'] = []
        if fault == 'wrong_frame': bad['frames'][-1]['layout']['to_allocate'] += 4
        if fault == 'wrong_boundary': bad['callees'][2]['known_incoming_boundary'] = 32
        try:
            validate_trace(bad, False)
        except AssertionError:
            refusals.append(fault)
        else:
            raise AssertionError('invalid trace accepted: '+fault)
    result = {'driver_cells': len(reports), 'body_verdicts': sum(r['body_verdicts'] for r in reports),
              'reports': reports, 'observed_cells': 4, 'target_allocations': 16,
              'frame_model_checks': 60, 'refused_controls': refusals}
    (d.ROOT/'build/gcc3-uref-stack-audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('8/8 full-TU audits;272 body verdicts;4 raw observational controls;16 allocations;60 frame layouts;3 refusals')

if __name__ == '__main__':
    main()
