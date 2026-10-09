#!/usr/bin/env python3
"""Audit three known SDM controls and identify late forwarding/early reloads."""
import hashlib,json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_reload_trace import function,instructions

def main():
    root=d.ROOT/'build/sdm-postreload';ledger=json.loads((root/'results.json').read_text())
    family=ledger['families']['SDM'];base=root/'SDM/baseline/candidate.o'
    assert base.read_bytes()==(d.ROOT/'build/tc_out/src_fax_SDM.c.o').read_bytes()
    metadata=inspect(base);report=[]
    for label,cell in family['cells'].items():
        folder=root/'SDM'/label;obj=folder/'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash']
        assert hashlib.sha256((folder/'SDM.c').read_bytes()).hexdigest()==cell['source_hash']
        names=sorted(d.b.sizes(str(obj)));assert names==cell['functions']==sorted(d.b.sizes(str(base)))
        live={n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n))) for n in names}
        assert live==cell['verdicts'] and not cell.get('gains') and not cell.get('losses')
        changed=[n for n in names if d.b.body(str(obj),n)!=d.b.body(str(base),n)]
        assert set(changed)<={'SDM_descrambler'}
        current=inspect(obj)
        for key in ('records','allocated','nobits','relocations'):assert current[key]==metadata[key]
        stages={}
        for stage in ('20.combine','25.greg','26.postreload','35.mach'):
            text=function((folder/('SDM.c.'+stage)).read_text(),'SDM_descrambler')
            if stage=='25.greg':
                marker=';; Start of basic block 0,';assert text.count(marker)==1;text=text[text.index(marker):]
            stream=instructions(text)
            stages[stage]={'HI_memory_UIDs':[u for u,r in stream.items() if 'mem:HI' in str(r)],'patterns':stream}
        late={'mask-postincrement':56,'compound-postincrement':57}.get(label)
        if late:
            before=stages['25.greg']['patterns'][late];after=stages['26.postreload']['patterns'][late]
            assert before[2][0]=='mem:HI' and after[1][0]==after[2][0]=='reg:HI'
            assert after[1][1]==after[2][1]=='0'
        if label=='compound-postincrement':
            assert 118 not in stages['20.combine']['patterns']
            assert stages['25.greg']['patterns'][118][2][0]=='mem:HI'
            assert 118 in stages['35.mach']['HI_memory_UIDs']
        report.append({'cell':label,'body_grades':len(live),'grade':live['SDM_descrambler'],'changed':changed,'late_read_UID':late,'stages':stages})
    output={'cells':len(report),'body_grades':sum(r['body_grades'] for r in report),'raw_baselines':1,'gains':0,'losses':0,'rows':report}
    (root/'audit.json').write_text(json.dumps(output,indent=2)+'\n')
    print('SDM: 3 known cells / 9 live grades / 1 raw baseline; 0 gains / 0 losses')
    print('Both postincrement late reads become AX selfcopies; compound early reload UID118 survives')

if __name__=='__main__':main()
