#!/usr/bin/env python3
"""Require only the witnessed DTMF pool byte and positioned consumer to change."""
from collections import Counter
import json
from pathlib import Path
import gcc3_value_carriers_audit as a
import storage_content_audit as content

ROOT=Path(__file__).resolve().parents[1]
OLD=('merge-entry',4,b'\x6f\x12\x03\x3b',0)
NEW=('merge-entry',4,b'\x6e\x12\x03\x3b',0)

def main():
    root=ROOT/'build/recovery-dtmf-level-cutoff';folder=root/'Dtmf'
    cells=json.loads((root/'results.json').read_text())['families']['Dtmf']['cells']
    assert cells['baseline']['baseline_reproduced']
    before=folder/'baseline/candidate.o';after=folder/'original-cutoff/candidate.o'
    old,new=a.inspect(before),a.inspect(after)
    for key in ('records','nobits','relocations','text_positions'):
        assert old[key]==new[key],key
    modified=[]
    for section,payload in old['allocated'].items():
        x=bytes.fromhex(payload);y=bytes.fromhex(new['allocated'][section]);assert len(x)==len(y)
        modified.extend((section,i,x[i],y[i]) for i in range(len(x)) if x[i]!=y[i])
    assert len(modified)==1 and modified[0][0]=='.rodata.cst4' and modified[0][2:]==(0x6f,0x6e),modified
    assert cells['original-cutoff']['changed_bodies']==['dtmf_test']
    assert not cells['original-cutoff']['gains'] and not cells['original-cutoff']['losses']
    functions=cells['baseline']['functions'];assert len(functions)==4
    positioned=[]
    for name in functions:
        left=a.d.b.body(str(before),name);right=a.d.b.body(str(after),name)
        assert left[0]==right[0] and left[1].keys()==right[1].keys(),name
        for at,dest in left[1].items():
            if dest!=right[1][at]:
                assert name=='dtmf_test' and at==25
                assert dest==('R_386_32',OLD) and right[1][at]==('R_386_32',NEW)
                assert right[0][at-2:at]==b'\xd9\x05' # FLDS absolute memory
                positioned.append({'function':name,'offset':at,'before':OLD[2].hex(),'after':NEW[2].hex()})
    assert len(positioned)==1
    reference=a.d.b.body(a.d.b.BLOB,'dtmf_test')
    assert reference[1][35]==('R_386_32',NEW) and reference[0][33:35]==b'\xd9\x05'
    ref=content.inventory(Path(a.d.b.BLOB));baseline=content.inventory(before);fixed=content.inventory(after)
    assert ref['pools']['dtmf_test']==fixed['pools']['dtmf_test']
    assert ref['pools']['dtmf_test']!=baseline['pools']['dtmf_test']
    exact={label:sorted(n for n,v in c['verdicts'].items() if v[0]=='EXACT') for label,c in cells.items()}
    assert exact['baseline']==exact['original-cutoff'] and len(exact['baseline'])==2
    report={'full_TU_cells':2,'function_verdicts':8,'stable_bystanders':3,'exact_names':exact,
            'all_masked_instructions_equal':True,'metadata_bindings_nontext_relocations_equal':True,
            'positioned_consumer_change':positioned,'original_pool_consumer_count':sum(ref['pools']['dtmf_test'].values()),
            'original_FLDS_anchor':'ada31 instruction / ada33 relocation / .rodata.cst4+50c',
            'original_decision_anchor':'adadc FLDS total;adae6 FCOMP threshold;adaf5 JB below rejects',
            'known_negative_original_pool_mismatch_detected':True,'changed_allocated_byte':modified}
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('DTMF cutoff audit: 2 cells, 8 function verdicts, 3 stable bystanders; 1 pool byte and 1 positioned FLDS target restored; 8/8 original numeric consumers')

if __name__=='__main__':main()
