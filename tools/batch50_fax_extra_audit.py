#!/usr/bin/env python3
"""Audit additional bounded completed-fax full-TU source controls."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text()
exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
roots=['v27-descramble-api','v27-result-boundary','delete-id','vmi-loops','v21-status','sgd','v29-control','v21-control','budget']
reports={};gains={};count=0
for root in roots:
 out=d.ROOT/('build/batch50-'+root if root.startswith('v27-') else 'build/batch50-fax-'+root);r=json.loads((out/'results.json').read_text())
 for family,f in r['families'].items():
  if (root.startswith('v27-') and family not in ('V27r_int','V27r_prc')) or not f['cells']['baseline']['functions']:
   for label,c in f['cells'].items():
    assert (out/family/label/'candidate.o').read_bytes()==(out/family/'baseline/candidate.o').read_bytes()
    count+=1;reports[root+'/'+family+'/'+label]={'functions':0,'raw_identical':True}
   continue
  base=canonical(out/family/'baseline/candidate.o')
  for label,c in f['cells'].items():
   q=canonical(out/family/label/'candidate.o');count+=1
   assert q['records']==base['records'],(root,family,label,'metadata')
   assert q['objects']==base['objects'],(root,family,label,'named data')
   assert not c.get('losses'),(root,family,label,'losses')
   assert Counter(map(repr,q['text_relocations']))==Counter(map(repr,base['text_relocations'])),(root,family,label,'text relocations')
   for k in ('nontext','nontext_relocations','allocated_sizes'):assert q[k]==base[k],(root,family,label,k)
   if root.startswith('v27-'):
    allowed={'DescrambleDataV27'} if family=='V27r_int' else {'RxHdxDataV27','RxHdxPrtcolV27'} if family=='V27r_prc' else set()
    if family!='V27r_prc':assert (out/family/label/'candidate.o').read_bytes()==(out/family/'baseline/candidate.o').read_bytes(),(root,family,label,'unchanged consumer/callee raw')
   elif root=='delete-id':allowed={'FAXVMI_delete'} if family=='faxvmi' else {'GetT30FrameIDFromBuffer'}
   elif root=='vmi-loops':allowed={'FAXVMI_create','FAXVMI_control'}
   elif root=='v21-status':allowed={'V21TX_status'}
   elif root=='sgd':allowed={'SGD_correlate','SGD_pattern_det'}
   elif root=='v29-control':allowed={'V29RX_control'}
   elif root=='v21-control':allowed={'V21TX_control'}
   else:
    from batch50_fax_budget import NAMES
    allowed=set(NAMES[family])
   assert set(c.get('changed_bodies',[]))<=allowed,(root,family,label,'bystanders')
   for n in c.get('gains',[]):gains[n]=b.sizes(b.BLOB)[n]
   reports[root+'/'+family+'/'+label]={'functions':len(c['functions']),'named_data':len(q['objects']),'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[])}
(d.ROOT/'build/batch50-fax-extra-audit.json').write_text(json.dumps({'cells':count,'gains':gains,'reports':reports},indent=2)+'\n')
print(count,'valid complete-TU cells; unchanged metadata/data/relocations; no bystander changes or exact losses;',len(gains),'unique exact gains;',sum(gains.values()),'blob bytes')
