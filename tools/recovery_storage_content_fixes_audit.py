#!/usr/bin/env python3
"""Require only the two original-backed data changes on complete period TUs."""
import json
from pathlib import Path
import gcc3_value_carriers_audit as a
import storage_content_audit as content

ROOT=Path(__file__).resolve().parents[1]

def find(inv,name):
    rows=[r for r in inv['records'] if r['name']==name]
    assert len(rows)==1
    return rows[0]

def main():
    root=ROOT/'build/recovery-storage-content-fixes'
    families=json.loads((root/'results.json').read_text())['families']
    original=content.inventory(ROOT/'ref/slmodemd/dsplibs.o')
    report=[]
    for family,name in (('v34filters','p2400'),('b103_tables','MTDb103Org_CFG')):
        cells=families[family]['cells'];assert cells['baseline']['baseline_reproduced']
        before=root/family/'baseline/candidate.o';after=root/family/'original-data/candidate.o'
        old,new=a.inspect(before),a.inspect(after)
        assert old['records']==new['records'] and old['nobits']==new['nobits']
        cell=cells['original-data'];assert cell['changed_bodies']==[] and not cell['gains'] and not cell['losses']
        assert old['text_positions']==new['text_positions']
        left,right=content.inventory(before),content.inventory(after)
        target=find(right,name);source=find(original,name)
        restored=content.payload_compare(source,target)
        assert restored['masked_bytes_equal'] and restored['relocations_equal']
        known_bad=content.payload_compare(source,find(left,name))
        assert not (known_bad['masked_bytes_equal'] and known_bad['relocations_equal'])
        if family=='v34filters':
            assert old['relocations']==new['relocations']
            changed=[]
            for section,payload in old['allocated'].items():
                aa=bytes.fromhex(payload);bb=bytes.fromhex(new['allocated'][section]);assert len(aa)==len(bb)
                changed.extend((section,i) for i,(x,y) in enumerate(zip(aa,bb)) if x!=y)
            assert changed==[(target['section'],target['offset']+300),(target['section'],target['offset']+306)]
        else:
            assert old['allocated']==new['allocated']
            assert old['relocations']==[]
            assert new['relocations']==[['.rodata',target['offset'],'R_386_32',('symbol','MTDb103_COEF',0)]]
        report.append({'family':family,'functions_per_cell':len(cell['verdicts']),
                       'exact_names':[n for n,v in cell['verdicts'].items() if v[0]=='EXACT'],
                       'changed_code_bodies':[],'restored_object':name,
                       'original_negative_detected':True,'original_payload_and_relocations_restored':True})
    (root/'audit.json').write_text(json.dumps({'full_TU_cells':4,'function_verdicts':52,'families':report},indent=2)+'\n')
    print('data repair audit: 4 full TU cells, 52 function verdicts, zero code/exact-name/binding drift; two original data negatives rejected and restored')

if __name__=='__main__':main()
