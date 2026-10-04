#!/usr/bin/env python3
"""Replay real SI scratch choices, including unchanged TRN's compilation history."""
import copy
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def transfer(event,start):
    assert event['constraint']=='r' and event['mode']=='SImode'
    s=event['eligibility'];eligible=[]
    for reg in s['class_members']:
        if s['fixed'][reg] or not s['call_used'][reg] and not s['ever_live'][reg]:continue
        if reg in (6,20) and (not s['reload_completed'] or s['frame_pointer_needed']):continue
        if reg in s['live'] or reg in event['excluded']:continue
        assert 0<=reg<8
        eligible.append(reg)
    for i in range(53):
        index=(start+i)%53;reg=event['allocation_order'][index]
        if reg in eligible:return reg,(index+1)%53
    return None,0

def main():
    r=json.loads((ROOT/'build/gcc3-alignment-scratch/results.json').read_text())
    assert len(r['cells'])==4
    modeled=0;observed=0;targets={}
    for label,cell in r['cells'].items():
        assert cell['raw_object_unchanged']
        for event in cell['trace']['events']:
            observed+=1
            if event['constraint']!='r' or event['mode']!='SImode':continue
            modeled+=1
            assert transfer(event,event['cursor_before'])==(event['selected'],event['cursor_after'])
            if event['function']=='generateTRN1d':
                assert label not in targets
                targets[label]=event
    assert len(targets)==4
    base=targets['V90Phase3Modulator/baseline']
    for label,event in targets.items():
        assert event['eligibility']==base['eligibility'] and event['excluded']==base['excluded']
        phase='phase-1-' in label
        assert event['cursor_before']==(2 if phase else 3)
        assert event['selected']==(2 if phase else 0)
    refused=[]
    bad=copy.deepcopy(base);bad['mode']='unsupported'
    try:transfer(bad,bad['cursor_before'])
    except AssertionError:refused.append('unsupported-mode')
    else:raise AssertionError('unsupported mode accepted')
    bad=copy.deepcopy(base);bad['selected']=2
    try:assert transfer(bad,bad['cursor_before'])==(bad['selected'],bad['cursor_after'])
    except AssertionError:refused.append('corrupt-selection')
    else:raise AssertionError('corrupt selection accepted')
    result={'observed':observed,'modeled':modeled,'outside_model':observed-modeled,'targets':targets,
            'refusals':refused,'diagnostic_only':True}
    (ROOT/'build/gcc3-alignment-scratch/model.json').write_text(json.dumps(result,indent=2)+'\n')
    print(modeled,'/',observed,'actual SI/general scratch choices reproduced;4 target events;same eligibility;2 refusals')
if __name__=='__main__':main()
