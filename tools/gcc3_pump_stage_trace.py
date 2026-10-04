#!/usr/bin/env python3
"""Assert a pinned V90 pump trace using existing dumps; never compile source."""
import hashlib,json,re
from pathlib import Path
from gcc3_value_carriers_audit import function_chunk
ROOT=Path(__file__).resolve().parents[1]
REV='14769433'
DOMAIN='existing baseline: shared-zero-return and Sd table placement only'
OUT=ROOT/'build/gcc3-sample-ownership'
UIDS=(13,98,101,103,104,86,122,123,1360,1364,1366,1409,1353,1355,1356,
      1411,1415,1416,1417,1423,1424,1692,1780,1781,1782,1783,1784,1785,
      1822,1863,1887,1914,1981,2023)

def forms(text,stage):
    pieces=re.split(r'(?=^\((?:insn|jump_insn|call_insn|code_label|note|barrier)(?::\w+)?\b)',text,flags=re.M)
    found={};duplicates=0
    for position,piece in enumerate(pieces):
        match=re.match(r'\((\w+)(?::\w+)? (\d+)',piece)
        if match:
            uid=int(match[2])
            if uid in found:
                assert stage=='08.gcse', ('unexpected duplicate UID',stage,uid)
                duplicates+=1
            found[uid]={'position':position,'form':piece.rstrip()}
    assert duplicates==(841 if stage=='08.gcse' else 0), (stage,duplicates)
    return found

def has_branch(form,condition,label):
    return bool(re.search(r'\(if_then_else \('+condition+r'\b',form)) and '(label_ref '+str(label)+')' in form

def sd_remote(stage):
    return (has_branch(stage['98']['form'],'leu',1887)
            and stage['101']['position']>stage['86']['position'])

def main():
    manifest=json.loads((OUT/'results.json').read_text())
    assert manifest['revision']==REV, 'unreviewed revision'
    baseline=manifest['families']['V90Phase3Modulator']['cells']['baseline']
    assert baseline['baseline_reproduced'] and baseline['compile_exit']==0
    folder=OUT/'V90Phase3Modulator/baseline';report={}
    assert hashlib.sha256((folder/'candidate.o').read_bytes()).hexdigest()==baseline['object_hash']
    assert hashlib.sha256((folder/'V90Phase3Modulator.cpp').read_bytes()).hexdigest()==baseline['source_hash']
    for path in sorted(folder.glob('V90Phase3Modulator.cpp.*')):
        if '.00.' in path.name:continue
        stage=path.name.split('.cpp.')[1]
        allforms=forms(function_chunk(path,'::generateV90Symbol('),stage)
        report[stage]={str(uid):allforms[uid] for uid in UIDS if uid in allforms}
    assert len(report)==30, ('unexpected dump denominator',len(report))
    def raw(stage,uid):return report[stage][str(uid)]['form']
    initial=report['01.rtl']
    assert '(const_int 28 [0x1c])' in raw('01.rtl',1360) and '(const_int 0 [0x0])' in raw('01.rtl',1360)
    assert '(reg/v:SI 60 [ sample ])' in raw('01.rtl',1364) and '(const_int 0 [0x0])' in raw('01.rtl',1364)
    assert '(label_ref 1411)' in raw('01.rtl',1366)
    assert '(reg/v:SI 60 [ sample ])' in raw('01.rtl',1415)
    assert '(reg:SI 313)' in raw('01.rtl',1416)
    assert '(reg:SI 58 [ <result> ])' in raw('01.rtl',1424)
    assert has_branch(raw('01.rtl',98),'gtu',86)
    assert initial['98']['position']<initial['101']['position']<initial['86']['position']
    for stage in report:
        if stage<'31.bbro':assert not sd_remote(report[stage]), ('unexpected early inversion',stage)
    first_remote=next(stage for stage in report if sd_remote(report[stage]))
    assert first_remote=='31.bbro'
    assert sd_remote(report['35.mach'])
    assert '(const_int 5000 [0x1388])' in raw('12.bp',98) and '(const_int 5000 [0x1388])' in raw('31.bbro',98)
    assert '(label_ref:SI 103)' in raw('31.bbro',101)
    assert raw('01.rtl',104).count('(label_ref:SI ')==6 and raw('31.bbro',104).count('(label_ref:SI ')==6
    first_epilogue=next(stage for stage in report if '1781' in report[stage]);assert first_epilogue=='27.flow2'
    first_zero_peephole=next(stage for stage in report if '1822' in report[stage]);assert first_zero_peephole=='28.peephole2'
    assert '(const_int 0 [0x0])' in raw(first_zero_peephole,1822) and '(clobber (reg:CC 17 flags))' in raw(first_zero_peephole,1822)
    assert report['30.rnreg']['1411']['position']>report['31.bbro']['1411']['position']
    assert '(label_ref 1411)' in raw('31.bbro',1914)
    first_final_label=next(stage for stage in report if '1981' in report[stage]);assert first_final_label=='34.stack'
    assert '1411' not in report[first_final_label] and '(label_ref 1981)' in raw(first_final_label,2023)
    # Predicates must fire on the measured later stage and refuse the initial
    # direction, a changed destination, and a branch whose table remains local.
    positive=report['31.bbro'];assert sd_remote(positive) and not sd_remote(initial)
    changed={**positive,'98':{**positive['98'],'form':positive['98']['form'].replace('(label_ref 1887)','(label_ref 86)')}}
    assert not sd_remote(changed)
    local={**positive,'101':{**positive['101'],'position':positive['86']['position']-1}}
    assert not sd_remote(local)
    summary={'revision':REV,'domain':DOMAIN,'dump_count':len(report),'functions':1,
             'shared_return_first_saved_stage':'01.rtl','hardware_epilogue_first_saved_stage':first_epilogue,
             'zero_peephole_first_saved_stage':first_zero_peephole,'remote_Sd_first_saved_stage':first_remote,
             'final_return_label_first_saved_stage':first_final_label,'predicate_positive_controls':1,'predicate_negative_controls':3,
             'no_new_builds':True,'no_unique_stack_pass_cause_claim':True}
    (OUT/'pump-stage-trace.json').write_text(json.dumps(report,indent=2)+'\n')
    (OUT/'pump-stage-trace-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary))

if __name__=='__main__':main()
