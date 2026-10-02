#!/usr/bin/env python3
"""Audit all three shared MRF result cells, including unchanged callers."""
from pathlib import Path
import json,re
import playbook_small_patterns as driver
source=(driver.ROOT/'tools/playbook_b103_tx_audit.py').read_text()
exec(source[source.index('from elftools'):source.index('runs=')])
root=driver.ROOT/'build/playbook-mrf-result'
records=json.loads((root/'results.json').read_text())
reports={};compiles=functions=0
for family,row in records['families'].items():
 folder=root/family
 baseline=fullinspect(folder/'baseline/candidate.o')
 reports[family]={}
 functions+=len(row['cells']['baseline']['functions'])
 for label,cell in row['cells'].items():
  report=fullinspect(folder/label/'candidate.o')
  for key in ('records','objects','allocated_sizes','nontext','nontext_relocations'):
   assert report[key]==baseline[key],(family,label,key)
  changed=cell.get('changed_bodies',[])
  assert changed==(['FPM_MRF_filter'] if family=='fpm_mrf' and label!='baseline' else []),(family,label,changed)
  reports[family][label]=report;compiles+=1
 assert (folder/'unsigned-result/candidate.o').read_bytes()==(folder/'wide-result/candidate.o').read_bytes(),family
 if family=='fpm_mrf':
  old=b.body(str(folder/'baseline/candidate.o'),'FPM_MRF_filter')
  new=b.body(str(folder/'unsigned-result/candidate.o'),'FPM_MRF_filter')
  assert len(old[0])==len(new[0])==446
  assert [(i,x,y) for i,(x,y) in enumerate(zip(old[0],new[0])) if x!=y]==[(330,191,183),(418,191,183)]
  assert old[1:]==new[1:]
(root/'complete-object-audit.json').write_text(json.dumps(reports,indent=2,default=str)+'\n')
assert compiles==30 and functions==80
print('MRF result:30 compiles/80 functions;two extension changes;77 caller bodies unchanged;metadata/data/nontext/relocations agree;unsigned/wide raw equal')
