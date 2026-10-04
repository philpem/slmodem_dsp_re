#!/usr/bin/env python3
"""Check full-TU metadata, data, relocations and all emitted bystanders."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

def audit(package):
 out=d.ROOT/'build'/package;r=json.loads((out/'results.json').read_text());reports=[]
 for family,fr in r['families'].items():
  bp=out/family/'baseline/candidate.o';base=inspect(bp);names=d.b.sizes(str(bp))
  assert fr['cells']['baseline']['baseline_reproduced']
  for label,c in fr['cells'].items():
   p=out/family/label/'candidate.o';got=inspect(p)
   for k in ('records','allocated','nobits','relocations'):assert got[k]==base[k],(package,family,label,k)
   assert set(d.b.sizes(str(p)))==set(names)
   changed=[n for n in names if d.b.body(str(p),n)!=d.b.body(str(bp),n)]
   assert set(changed)==set(c.get('changed_bodies',[])),(family,label,changed)
   allowed={'RxHdxDataV'+family[1:3]} if family in ('V17r_prc','V27r_prc','V29r_prc') else set()
   if package=='next20-count-cross' and family=='V27r_prc':allowed.add('RxHdxPrtcolV27')
   if package=='next20-no-carrier-mask':allowed={'TxNoCarrierV'+family[1:3]}
   assert set(changed)<=allowed,(family,label,changed)
   if package=='next20-count-cross' and family not in ('V27r_prc','V29r_prc'):assert p.read_bytes()==bp.read_bytes(),(family,label,'raw unchanged consumers')
   assert not c.get('losses',[])
   reports.append(dict(family=family,label=label,emitted_functions=len(names),common_verdicts=len(c['verdicts']),changed=changed,gains=c.get('gains',[]),losses=[],metadata_data_BSS_relocations_unchanged=True))
 result=dict(cells=len(reports),emitted_body_comparisons=sum(c['emitted_functions'] for c in reports),common_verdicts=sum(c['common_verdicts'] for c in reports),reports=reports)
 (out/'complete-tu-audit.json').write_text(json.dumps(result,indent=2)+'\n')
 print(package,result['cells'],result['common_verdicts'],[(c['family'],c['label'],c['gains']) for c in reports if c['gains']])
if __name__=='__main__':
 import sys
 for package in sys.argv[1:]:audit(package)
