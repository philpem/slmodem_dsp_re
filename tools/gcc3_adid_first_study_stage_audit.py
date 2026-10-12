#!/usr/bin/env python3
"""Audit raw-equal ADID late-pass publication and clearing-store traces."""
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_mechanism_scratch_observe import validate_trace

MARKER = ';; Function void V90AutoDigitalImpDetector::porcessFirstStudy()'
STAGES = ('01.rtl', '20.combine', '25.greg', '26.postreload', '27.flow2',
          '28.peephole2', '29.ce3', '30.rnreg', '31.bbro', '33.sched2', '34.stack')

def predict(event, cursor):
    """Bounded replay of recog.c's scalar SI/QI target searches, no compiler writes."""
    assert event['mode'] in ('SImode', 'QImode')
    # The observer already unions liveness across the requested instruction span.
    assert 0 <= event['from'] <= event['to'] < 5
    state = event['eligibility']
    order = event['allocation_order']
    assert len(order) == 53
    for step in range(53):
        index = (cursor+step) % 53
        reg = order[index]
        if reg not in range(8) or (event['mode'] == 'QImode' and reg > 3):
            continue
        if state['fixed'][reg] or reg not in state['class_members']:
            continue
        if not state['call_used'][reg] and not state['ever_live'][reg]:
            continue
        if reg == 6 and (not state['reload_completed'] or state['frame_pointer_needed']):
            continue
        if reg in state['live'] or reg in event['excluded']:
            continue
        return reg, (index+1) % 53
    return None, 0

def inspect(text):
    assert MARKER in text, 'missing target function'
    fn = text.split(MARKER, 1)[1].split(';; Function ', 1)[0]
    blocks = re.split(r'\n(?=\((?:insn|jump_insn|note|code_label|barrier))', fn)
    publications = [b for b in blocks if '(set (mem' in b
                    and 'const_int 10252 [0x280c]' in b]
    middle = []
    for b in blocks:
        if '(set (mem' not in b:
            continue
        uid = re.match(r'\(insn[^ ]* (\d+)', b)
        if not uid:
            continue
        # The initial expansion computes the address in separate instructions;
        # later stages retain the ucode origin on the address pseudo.
        if 'ucode' not in b and int(uid.group(1)) not in (486, 497, 508):
            continue
        offsets = [n for n in (4096, 7168, 37144)
                   if 'const_int '+str(n)+' ' in b]
        if offsets:
            assert len(offsets) == 1
            middle.append({'uid': int(uid.group(1)), 'offset': offsets[0]})
    return {'publication_stores': len(publications), 'middle_stores': middle,
            'crossjump_reported': 'Cross jumping from bb 18 to bb 19; 1 common insns' in fn}

def main():
    root = d.ROOT/'build/gcc3-adid-first-study-stages'
    ledger = json.loads((root/'results.json').read_text())
    family = ledger['families']['V90AutoDigitalImpDetector']
    cell = family['cells']['baseline']
    assert cell['compile_exit'] == 0 and cell['baseline_reproduced']
    folder = root/'V90AutoDigitalImpDetector/baseline'
    assert (folder/'candidate.o').read_bytes() == (root/'V90AutoDigitalImpDetector/retained.o').read_bytes()
    rows = {}
    for stage in STAGES:
        row = inspect((folder/('V90AutoDigitalImpDetector.cpp.'+stage)).read_text())
        before_merge = int(stage[:2]) < 29
        assert row['publication_stores'] == (2 if before_merge else 1), (stage, row)
        order = [v['offset'] for v in row['middle_stores']]
        assert order == ([7168, 4096, 37144] if int(stage[:2]) < 33 else [4096, 7168, 37144]), (stage, order)
        rows[stage] = row
    assert rows['29.ce3']['crossjump_reported']
    observation = json.loads((d.ROOT/'build/gcc3-adid-first-study-scratch/results.json').read_text())
    observed = observation['cells']['V90AutoDigitalImpDetector/baseline']
    assert observed['raw_object_unchanged'] and observed['object_sha256'] == cell['object_hash']
    trace = observed['trace']
    validate_trace(trace, 'porcessFirstStudy')
    targets = [e for e in trace['events'] if e['function'] == 'porcessFirstStudy']
    assert len(targets) == 6
    publications = [e for e in targets if e['insn_uid'] in (345, 326)]
    assert [e['insn_uid'] for e in publications] == [345, 326]
    assert all(e['mode'] == 'QImode' and e['selected'] == 0 for e in publications)
    assert all(e['eligibility']['live'] == [4, 6, 7, 11] for e in publications)
    first = trace['events'].index(targets[0])
    predecessor = trace['events'][first-1]
    assert predecessor['function'] == 'updateUrefAlt'
    assert predecessor['cursor_after'] == targets[0]['cursor_before'] == 50
    # First validate the source-derived model against all six observed searches.
    for event in targets:
        assert predict(event, event['cursor_before']) == (event['selected'], event['cursor_after'])
    outcomes = []
    for incoming in range(53):
        cursor = incoming
        picks = []
        for event in targets:
            reg, cursor = predict(event, cursor)
            picks.append(reg)
        outcomes.append(picks)
    assert all(picks == [6, 0, 6, 0, 0, 0] for picks in outcomes)
    report = {'revision': ledger['revision'], 'raw_equal_complete_tus': 1,
              'stages': rows, 'scratch_events': len(trace['events']),
              'target_scratch_events': len(targets),
              'validated_model_searches': 6, 'modeled_entry_cursors': 53,
              'modeled_selection_sequences': outcomes,
              'target_events': [{k: e[k] for k in ('insn_uid', 'cursor_before', 'cursor_after', 'selected')}
                                for e in targets],
              'source_adopted': False, 'exact_gains': 0, 'exact_losses': 0}
    (d.ROOT/'build/adid-first-study-stage-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('ADID stage audit: 1 raw-equal TU; 11 stages; 2→1 publication stores; '
          'count/sum→sum/count at sched2; %d scratch events, 6 target events; '
          '53/53 incoming cursors converge' % len(trace['events']))

if __name__ == '__main__':
    main()
