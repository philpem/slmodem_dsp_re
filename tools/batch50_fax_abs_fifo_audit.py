#!/usr/bin/env python3
"""Full-TU proof for original FIFO create and abs boundary controls."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text()
exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
out=d.ROOT/'build/batch50-fax-abs-fifo';r=json.loads((out/'results.json').read_text());reports={};gains={};count=0
for family,f in r['families'].items():
 base=canonical(out/family/'baseline/candidate.o')
 for label,c in f['cells'].items():
  q=canonical(out/family/label/'candidate.o');count+=1
  for k in ('records','objects','nontext','nontext_relocations','allocated_sizes'):assert q[k]==base[k],(family,label,k)
  allowed={'FIFO_create'} if family=='fifo' else {'GetSNRV21'}
  assert set(c.get('changed_bodies',[]))<=allowed,(family,label,'bystanders')
  assert not c.get('losses'),(family,label,'loss')
  before=Counter(map(repr,base['text_relocations']));after=Counter(map(repr,q['text_relocations']))
  if family=='fifo' and 'cfg-copy' in label:
   assert not after-before,(family,label,'added relocations')
   assert before-after==Counter({"('R_386_32', ('symbol', 'FIFO_CFG', 2))":1}),(family,label,'removed narrow size read')
  else:assert before==after,(family,label,'text targets')
  for n in c.get('gains',[]):gains[n]=b.sizes(b.BLOB)[n]
  reports[family+'/'+label]={'functions':len(c['functions']),'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'removed_text_relocations':dict(before-after),'verdicts':{n:c['verdicts'][n] for n in allowed}}
(out/'full-audit.json').write_text(json.dumps({'cells':count,'gains':gains,'reports':reports},indent=2)+'\n')
print(count,'valid full-TU cells; identical metadata/nameddata/nontext/allbystanders; one original merged config read replaces redundant FIFO_CFG+2 load;',len(gains),'gains',sum(gains.values()),'bytes; no losses')
