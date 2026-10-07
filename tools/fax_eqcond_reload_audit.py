#!/usr/bin/env python3
"""Recompute EQ-conditioning full-TU controls from live objects."""
import hashlib,json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
import gcc3_reload_trace as rtl
TARGET='TxHdxEQCondV27'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    reports=[]
    original=d.b.body(d.b.BLOB,TARGET)
    for package in ['fax-eqcond-reloads','fax-eqcond-lifetimes']:
        root=d.ROOT/'build'/package
        saved=json.loads((root/'results.json').read_text())
        family=saved['families']['V27t_prc'];cells=family['cells']
        basepath=root/'V27t_prc/baseline/candidate.o'
        base=inspect(basepath);names=sorted(d.b.sizes(basepath))
        assert len(names)==10
        assert sha(basepath)==family['retained_hash']
        assert cells['baseline']['baseline_reproduced']
        baseline_bodies={name:d.b.body(basepath,name) for name in names}
        for label,cell in cells.items():
            folder=root/'V27t_prc'/label;obj=folder/'candidate.o'
            assert cell['compile_exit']==0
            assert sha(obj)==cell['object_hash']
            assert sha(folder/'V27t_prc.c')==cell['source_hash']
            current=inspect(obj)
            for key in ['records','allocated','nobits','relocations']:
                assert current[key]==base[key],(package,label,key)
            assert sorted(d.b.sizes(obj))==names
            live={name:list(d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(obj,name))) for name in names}
            assert live==cell['verdicts'],(package,label,'saved verdict disagreement')
            changed=sorted(name for name in names if d.b.body(obj,name)!=baseline_bodies[name])
            assert changed==cell.get('changed_bodies',[]),(package,label,changed)
            assert not changed or changed==[TARGET]
            gains=[name for name in names if live[name][0]=='EXACT' and cells['baseline']['verdicts'][name][0]!='EXACT']
            losses=[name for name in names if live[name][0]!='EXACT' and cells['baseline']['verdicts'][name][0]=='EXACT']
            assert gains==cell.get('gains',[]) and losses==cell.get('losses',[])
            assert not gains and not losses
            reports.append({'package':package,'label':label,'size':len(d.b.body(obj,TARGET)[0]),'changed_bodies':changed,'gains':gains,'losses':losses,'metadata_data_bss_nontext_relocations_identical':True,'live_hashes_and_verdicts_match':True})
    trace=[]
    folder=d.ROOT/'build/fax-eqcond-reloads/V27t_prc/c1-m1-r1-p1'
    for stage,count in [('01.rtl',3),('06.cse',2)]:
        chunk=rtl.function((folder/('V27t_prc.c.'+stage)).read_text(),TARGET)
        stream=rtl.instructions(chunk)
        member=[uid for uid,pattern in stream.items() if '<variable>.countdown+0' in str(pattern)]
        rate=[uid for uid,pattern in stream.items() if '<variable>.rate+0' in str(pattern)]
        assert len(member)==count and len(rate)==2,(stage,member,rate)
        trace.append({'stage':stage,'countdown_access_instruction_uids':member,'rate_read_instruction_uids':rate})
    report={'rtl_member_access_trace':trace,'cells':len(reports),'live_body_verdicts':10*len(reports),'original_size':len(original[0]),'gains':0,'losses':0,'reports':reports}
    (d.ROOT/'build/fax-eqcond-reload-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({key:value for key,value in report.items() if key!='reports'}))
if __name__=='__main__':main()
