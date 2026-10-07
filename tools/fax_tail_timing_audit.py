#!/usr/bin/env python3
"""Recheck live fax timing controls and their whole-TU metadata."""
import hashlib,json
from pathlib import Path
import playbook_small_patterns as d
import gcc3_reload_trace as rtl
from gcc3_value_carriers_audit import inspect

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    root=d.ROOT/'build/fax-tail-timing';saved=json.loads((root/'results.json').read_text());rows=[]
    for family,data in saved['families'].items():
        basepath=root/family/'baseline/candidate.o';base=inspect(basepath);names=sorted(d.b.sizes(basepath))
        assert sha(basepath)==data['retained_hash'] and data['cells']['baseline']['baseline_reproduced']
        retained=d.ROOT/'build/tc_out'/(data['source_path'].replace('/','_')+'.o')
        assert basepath.read_bytes()==retained.read_bytes(), 'current production baseline drift'
        target='SDM_descrambler' if family=='SDM' else 'FSE_decision_eqtrn'
        base_bodies={name:d.b.body(basepath,name) for name in names}
        for label,cell in data['cells'].items():
            folder=root/family/label;obj=folder/'candidate.o'
            assert cell['compile_exit']==0 and sha(obj)==cell['object_hash']
            assert sha(folder/(family+'.c'))==cell['source_hash']
            observed=inspect(obj)
            for key in ['records','allocated','nobits','relocations']:assert observed[key]==base[key],(family,label,key)
            assert sorted(d.b.sizes(obj))==names
            live={name:list(d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(obj,name))) for name in names}
            assert live==cell['verdicts']
            changed=sorted(name for name in names if d.b.body(obj,name)!=base_bodies[name])
            assert changed==cell.get('changed_bodies',[])
            assert not changed or changed==[target]
            gains=[name for name in names if live[name][0]=='EXACT' and data['cells']['baseline']['verdicts'][name][0]!='EXACT']
            losses=[name for name in names if live[name][0]!='EXACT' and data['cells']['baseline']['verdicts'][name][0]=='EXACT']
            assert gains==cell.get('gains',[]) and losses==cell.get('losses',[]) and not gains and not losses
            rows.append({'family':family,'label':label,'body_verdicts':len(names),'changed_bodies':changed,'target_verdict':live[target],'target_size':len(d.b.body(obj,target)[0]),'nontext_data_metadata_relocations_identical':True,'live_hash_verdict_validation':True})
    trace=[]
    for label,stages in [('baseline',[('01.rtl',[37,47,53,56]),('06.cse',[37,47,56]),('20.combine',[38,56])]),('mask-postincrement',[('01.rtl',[37,47,56,59]),('06.cse',[37,47,56,59]),('20.combine',[38,47,56,59]),('24.lreg',[38,47,56,59]),('25.greg',[38,47,56,59]),('26.postreload',[38,47,59])])]:
        for stage,expected in stages:
            chunk=rtl.function((root/'SDM'/label/('SDM.c.'+stage)).read_text(),'SDM_descrambler')
            if stage=='25.greg':
                marker=';; Start of basic block 0,'
                assert chunk.count(marker)==1
                chunk=chunk[chunk.index(marker):]
            stream=rtl.instructions(chunk)
            accesses=[uid for uid,pattern in stream.items() if 'mem:HI' in str(pattern)]
            assert accesses==expected,(label,stage,accesses)
            if label=='mask-postincrement' and stage=='25.greg':assert stream[56][2][0]=='mem:HI'
            if label=='mask-postincrement' and stage=='26.postreload':
                assert stream[56][1][0]==stream[56][2][0]=='reg:HI'
                assert stream[56][1][1]==stream[56][2][1]=='0'
            trace.append({'label':label,'stage':stage,'HI_data_access_uids':accesses,'UID56_pattern':stream.get(56)})
    report={'rtl_trace':trace,'cells':len(rows),'live_body_verdicts':sum(row['body_verdicts'] for row in rows),'gains':0,'losses':0,'rows':rows}
    (d.ROOT/'build/fax-tail-timing-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({key:value for key,value in report.items() if key!='rtl_trace'}))
if __name__=='__main__':main()
