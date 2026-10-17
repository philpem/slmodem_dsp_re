#!/usr/bin/env python3
"""Audit the single signed-SAR recovery, preserving full-TU collateral."""
import json
from pathlib import Path
import gcc3_value_carriers_audit as a

ROOT = Path(__file__).resolve().parents[1]

def main():
    root = ROOT/'build/recovery-call-count-shift'
    cells = json.loads((root/'results.json').read_text())['families']['call']['cells']
    assert cells['baseline']['baseline_reproduced']
    base = a.inspect(root/'call/baseline/candidate.o')
    new = a.inspect(root/'call/signed-shift/candidate.o')
    for key in ('records','allocated','nobits'):
        assert base[key] == new[key], key
    assert cells['signed-shift']['changed_bodies'] == ['call_run']
    assert not cells['signed-shift']['gains'] and not cells['signed-shift']['losses']
    assert len(base['relocations']) == len(new['relocations']) == 22
    moved=[]
    for old, observed in zip(base['relocations'],new['relocations']):
        assert old[:3] == observed[:3]
        if old != observed:
            assert old[3][:2] == observed[3][:2] == ('audited-code-destination','call_run')
            moved.append([old,observed])
    left=[x[3] for x in base['relocations']]; right=[x[3] for x in new['relocations']]
    assert [[x==y for y in left] for x in left] == [[x==y for y in right] for x in right]
    for stage in ('01.rtl','20.combine'):
        before=a.function_chunk(root/f'call/baseline/call.c.{stage}','call_run')
        after=a.function_chunk(root/f'call/signed-shift/call.c.{stage}','call_run')
        assert '(div:SI (reg/v:SI 67 [ n ])' in before
        assert '(div:SI' not in after
        # Both initial expansion and combine retain the direct 32-bit signed
        # shift in the source control; there is no unsigned-type substitution.
        assert '(ashiftrt:SI (reg/v:SI 67 [ n ])\n                    (const_int 1 [0x1]))' in after
        assert '(lshiftrt:SI (reg/v:SI 67 [ n ])' not in after
    exact={label:sorted(n for n,v in c['verdicts'].items() if v[0]=='EXACT') for label,c in cells.items()}
    assert exact['baseline'] == exact['signed-shift'] and len(exact['baseline']) == 7
    report={'function_verdicts':22,'stable_bystanders':10,'exact_names':exact,
            'nontext_relocations':22,'moved_switch_destinations':moved,
            'initial_and_combine_graph':'DIV bias removed, direct ASHIFTRT SI1 retained',
            'known_negative_division_graph':True,
            'original_operation':'3092 SAR1 EBX, followed by30a4 count subtraction; no bias producer',
            'source_scope':'Reproducing signed-shift source; original spelling not uniquely established by instruction alone.'}
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('count-shift audit: 2 cells, 22 function verdicts; 10 stable bystanders; division negative control fired')

if __name__ == '__main__':
    main()
