#!/usr/bin/env python3
"""Complete objects, unchanged controls and exact-set audit for services batch."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
PACKAGES=('services-v22-trained','services-detector-capture','services-v32-silent-ring','services-tone-cursor','services-voice-count-sum')

def main():
 totals={'cells':0,'bodies':0,'gains':[],'losses':[],'raw_baselines':0,'changed_body_controls':0,'families':{}}
 for package in PACKAGES:
  base=d.ROOT/'build'/package;ledger=json.loads((base/'results.json').read_text())
  for name,family in ledger['families'].items():
   root=base/name;baseline=root/'baseline/candidate.o';base=inspect(baseline)
   names=family['cells']['baseline']['functions'];raw_bodies={n:d.b.body(str(baseline),n) for n in names}
   assert baseline.read_bytes()==(root/'retained.o').read_bytes()
   totals['raw_baselines']+=1;rows={}
   for label,cell in family['cells'].items():
    obj=root/label/'candidate.o';view=inspect(obj)
    for key in ('records','allocated','nobits','relocations'):assert view[key]==base[key],(package,label,key)
    changed=sorted(n for n in names if d.b.body(str(obj),n)!=raw_bodies[n])
    assert changed==sorted(cell.get('changed_bodies',[])),(package,label,'body count drift')
    assert not cell.get('losses',[]),(package,label,'exact loss')
    assert cell.get('gains',[])==(['voice_modem'] if package=='services-voice-count-sum' and label=='detector-plus-handler' else []),(package,label,'unexpected gain')
    totals['gains'].extend(cell.get('gains',[]))
    totals['cells']+=1;totals['bodies']+=len(names)
    totals['changed_body_controls']+=bool(changed)
    rows[label]={'changed':changed,'verdicts':cell['verdicts']}
   totals['families'][package]=rows
 assert totals['cells']==38 and totals['bodies']==402
 assert totals['raw_baselines']==5 and totals['changed_body_controls']>0,'detector never fired'
 target=totals['families']['services-v22-trained']['forward-1-backward-1-branch-1']['changed']
 assert 'RxTrained1200' in target and 'RxTrained2400' in target,'known source control not observed'
 assert totals['gains']==['voice_modem']
 out=d.ROOT/'build/services-return-audit.json';out.write_text(json.dumps(totals,indent=2)+'\n')
 print('Services full-TU audit:',totals['cells'],'cells;',totals['bodies'],'bodies;',totals['raw_baselines'],'raw baselines;',totals['changed_body_controls'],'changed-body controls; gains',totals['gains'],'zero losses')
if __name__=='__main__':main()
