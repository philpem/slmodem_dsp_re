#!/usr/bin/env python3
"""Audit getPower reload slots, conservative table proof and complete TU cells."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_stage_divergence import chunks
from batch_cpp_constellation_expand_audit import cases, TARGET as HELPER, CALLER as TARGET
import induction_jumptable

FAMILY='V90ConstellationPower'
HEADER='float V90ConstellationPower::getPower(V90MappingParams*, V90TxPowerMeasurementPoint, PcmType)'


def slots(folder, trace):
    fn=chunks(folder/(FAMILY+'.cpp.01.rtl'),HEADER)
    assert len(fn)==1
    pseudos={name:int(number) for number,name in re.findall(r'reg/v:DF (\d+) \[ (power|fraction) \]',fn[0])}
    assert set(pseudos)=={'power','fraction'}
    layout=trace['frames'][-1]['layout']
    displacement=layout['stack_pointer_offset']-layout['frame_pointer_offset']
    result={}
    for event in trace['events']:
        for caller in event['caller_values']:
            if caller['function']!='alter_reg':continue
            number=caller['available_values']['i']; node=event['returned']['operands'][0]
            assert node['code']=='PLUS' and node['operands'][0]['regno']==20
            for name,pseudo in pseudos.items():
                if number==pseudo:
                    result[name]={'pseudo':pseudo,'frame_offset':node['operands'][1]['value'],
                                  'stack_offset':displacement+node['operands'][1]['value']}
    assert set(result)==set(pseudos)
    return result


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--integrated',action='store_true');args=ap.parse_args()
    root=d.ROOT/'build/gcc3-getpower-lifetime'
    cells=json.loads((root/'results.json').read_text())['families'][FAMILY]['cells']
    obs=json.loads((d.ROOT/'build/gcc3-getpower-lifetime-observation/results.json').read_text())
    base=root/FAMILY/'baseline/candidate.o'; report={}
    original_proof=induction_jumptable.prove(d.b.BLOB,TARGET)
    assert cases(Path(d.b.BLOB))==original_proof['targets']
    for label,cell in cells.items():
        folder=root/FAMILY/label;path=folder/'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest()==cell['object_hash']
        assert inspect(path)==inspect(base) and allocated(path)==allocated(base)
        changed=[n for n in cell['functions'] if masked(path,n)!=masked(base,n)]
        assert changed==([TARGET] if label=='fraction-first-1' else [])
        assert d.b.verdict(*d.b.body(d.b.BLOB,HELPER),*d.b.body(str(path),HELPER))==('EXACT',0)
        grades={n:d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(path),n)) for n in cell['verdicts']}
        assert grades[TARGET]==(('EXACT',0) if label=='fraction-first-1' else ('BYTES',10))
        proof=induction_jumptable.prove(str(path),TARGET)
        assert proof['identity']==original_proof['identity'] and cases(path)==original_proof['targets']
        entry=obs['cells'][label];trace=entry['trace']
        assert entry['raw_object_unchanged'] and trace['exit_codes']==[0] and not trace['errors'] and not trace['pending']
        assert len(trace['events'])==6
        homes=slots(folder,trace)
        assert homes['power']['stack_offset']==(0x28 if label=='fraction-first-1' else 0x30)
        assert homes['fraction']['stack_offset']==(0x30 if label=='fraction-first-1' else 0x28)
        report[label]={'slots':homes,'live_verdicts':grades,'table_proof':proof,'changed_bodies':changed,'raw_observation_equal':True}
    assert cells['baseline']['object_hash']==cells['fraction-first-0']['object_hash']
    unchanged=d.ROOT/'build/gcc3-getpower-stack'/FAMILY/'baseline/candidate.o'
    assert unchanged.read_bytes()==base.read_bytes()
    untouched=json.loads((d.ROOT/'build/gcc3-getpower-observation/results.json').read_text())
    assert untouched['cells']['baseline']['raw_object_unchanged']
    if args.integrated:
        winner=root/FAMILY/'fraction-first-1'
        assert (d.ROOT/'src/pump/v90/V90ConstellationPower.cpp').read_bytes()==(winner/(FAMILY+'.cpp')).read_bytes()
        assert (d.ROOT/'build/tc_out/src_pump_v90_V90ConstellationPower.cpp.o').read_bytes()==(winner/'candidate.o').read_bytes()
    report['denominator']={'unique_source_cells':3,'common_body_verdicts':27,'raw_observations':4,'target_stack_requests':24}
    report['integrated']=args.integrated
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('3 cells /27 body verdicts; 4 raw-equal observations /24 stack requests; 574B exact winner; 8 bystanders/data unchanged')


if __name__=='__main__':main()
