#!/usr/bin/env python3
"""Full-TU and initial-RTL producer/consumer audit for split call owners."""
import json
from pathlib import Path
import gcc3_value_carriers_audit as a

ROOT = Path(__file__).resolve().parents[1]

def partition(rows):
    targets = [r[3] for r in rows]
    return [[x == y for y in targets] for x in targets]

def graph(path):
    text = a.function_chunk(path, 'call_run')
    # Bound to this revision's expansion: outer state63 produces the two
    # buffers, dp100 is copied from the incoming owner, inner state108 reloads.
    start = text.index('(insn 109 ')
    end = text.index('(insn 126 ', start)
    window = text[start:end]
    return {'outer_buffer_loads':window.count('reg/v/f:SI 63 [ st ]'),
            'direct_buffer_producers': '(and:SI' not in window,
            'inner_reload': '(insn 118 ' in window and 'reg/v/f:SI 100 [ dp ]' in window,
            'inner_answered_consumer':'reg/v/f:SI 108 [ st ]' in window and '<variable>.answered' in window}

def main():
    root = ROOT/'build/recovery-call-owner-split'
    cells = json.loads((root/'results.json').read_text())['families']['call']['cells']
    assert cells['baseline']['baseline_reproduced']
    base = a.inspect(root/'call/baseline/candidate.o')
    rows = []
    for label, cell in cells.items():
        obj = a.inspect(root/'call'/label/'candidate.o')
        for key in ('records','allocated','nobits'):
            assert obj[key] == base[key], (label,key)
        changed = []
        assert len(obj['relocations']) == len(base['relocations']) == 22
        for old, new in zip(base['relocations'],obj['relocations']):
            assert old[:3] == new[:3]
            if old != new:
                assert old[3][:2] == new[3][:2] == ('audited-code-destination','call_run')
                changed.append([old,new])
        assert partition(obj['relocations']) == partition(base['relocations'])
        assert cell.get('changed_bodies',[]) == ([] if label == 'baseline' else ['call_run'])
        assert not cell.get('gains') and not cell.get('losses')
        assert len(cell['verdicts']) == 11
        assert sum(v[0] == 'EXACT' for v in cell['verdicts'].values()) == 7
        rows.append({'cell':label,'function_verdicts':11,'exact_names':[n for n,v in cell['verdicts'].items() if v[0]=='EXACT'],
                     'stable_bystanders':10,'moved_switch_destinations':changed})
    observed = graph(root/'call/split-byte-address/call.c.01.rtl')
    assert observed == {'outer_buffer_loads':4,'direct_buffer_producers':True,'inner_reload':True,'inner_answered_consumer':True}, observed
    # Known-negative control is the retained short-pointer expansion: it must
    # contain the discarded low bit, so the direct-address detector fires.
    old = a.function_chunk(root/'call/split-owner/call.c.01.rtl','call_run')
    assert old.count('(udiv:SI') == 4
    assert '(const_int 2 [0x2])' in old
    combined = a.function_chunk(root/'call/split-owner/call.c.20.combine','call_run')
    assert '(and:SI' in combined and '(const_int -2 [0xfffffffe])' in combined
    report = {'cells':rows,'function_verdict_denominator':33,'stage':'01.rtl',
              'adopted_graph':observed,'known_negative_short_pointer_mask':True,
              'original_witness':{'outer_buffers':'2f38/2f42 loads from ESI;2f48/2f50 direct byte additions',
                                  'inner_owner':'2f56 reload [ECX+16] into EDI;2f5f answered consumer'}}
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('split-owner audit: 3 cells, 33 function verdicts; 10 stable bystanders each; direct-address negative control fired')

if __name__ == '__main__':
    main()
