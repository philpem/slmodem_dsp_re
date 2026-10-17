#!/usr/bin/env python3
"""Audit both diagnostic pass gates without making a flag-adoption claim."""
import json
import re
from pathlib import Path
import gcc3_value_carriers_audit as a

ROOT=Path(__file__).resolve().parents[1]

def chunk(root,label,stage):
    return a.function_chunk(root/'call'/label/('call.c.'+stage),'call_create')

def conflicts(text,reg):
    match=re.search(r'^;; '+str(reg)+r' conflicts: ([0-9 ]+)$',text,re.M)
    assert match
    return [int(n) for n in match[1].split()]

def main():
    root=ROOT/'build/recovery-call-create-ce2'
    cells=json.loads((root/'results.json').read_text())['families']['call']['cells']
    assert cells['baseline']['baseline_reproduced']
    base=a.inspect(root/'call/baseline/candidate.o')
    reports=[]
    for label,c in cells.items():
        obj=a.inspect(root/'call'/label/'candidate.o')
        for key in ('records','allocated','nobits'):
            assert obj[key]==base[key],(label,key)
        moved=[]
        assert len(obj['relocations'])==len(base['relocations'])==22
        for old,new in zip(base['relocations'],obj['relocations']):
            assert old[:3]==new[:3]
            if old!=new:
                assert old[3][:2]==new[3][:2]==('audited-code-destination','call_run')
                moved.append([old,new])
        old=[r[3] for r in base['relocations']];new=[r[3] for r in obj['relocations']]
        assert [[x==y for y in old] for x in old]==[[x==y for y in new] for x in new]
        expected=['IsPulseDialerReady','call_create','call_run'] if label=='diagnostic-no-if-conversion' else []
        assert c.get('changed_bodies',[])==expected
        assert not c.get('gains') and not c.get('losses')
        exact=sorted(n for n,v in c['verdicts'].items() if v[0]=='EXACT')
        assert exact==sorted(n for n,v in cells['baseline']['verdicts'].items() if v[0]=='EXACT')
        local=chunk(root,label,'24.lreg');greg=chunk(root,label,'25.greg')
        adjacent=bool(re.search(r'\(insn:HI 173 170 171 ',local))
        conflict=87 in conflicts(greg,86)
        assert adjacent==conflict==(label!='diagnostic-no-if-conversion')
        assert ('REG_DEAD (reg/v/f:SI 87 [ want ])' in re.search(r'\(insn:HI 173 .*?(?=\n\n)',local,re.S)[0]) == (label=='diagnostic-no-if-conversion')
        reports.append({'cell':label,'changed_bodies':expected,'stable_bodies':11-len(expected),
                        'exact_names':exact,'moved_switch_destinations':moved,
                        'copy_hoisted':adjacent,'want_result_conflict':conflict})
    assert (root/'call/baseline/candidate.o').read_bytes()==(root/'call/diagnostic-no-ce2/candidate.o').read_bytes()
    result={'function_verdicts':33,'cells':reports,'source_unchanged':True,'production_flag_adopted':False,
            'actual_gate_mapping':'Actual Gentoo rest_of_compilation:0822b68e flag_if_conversion gates21.ce2;0822b897 flag_if_conversion2 gates29.ce3',
            'conclusion':'Early pass hoists copies and causes overlap; disabling it removes conflict but copies still exist in different hard registers. This does not establish a uniquely original source or a flag fix.'}
    (root/'audit.json').write_text(json.dumps(result,indent=2)+'\n')
    print('call if-conversion audit: 3 cells, 33 function verdicts; late-pass raw negative and early-pass conflict positive fired; no adoption')

if __name__=='__main__':
    main()
