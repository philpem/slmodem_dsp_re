#!/usr/bin/env python3
"""Audit six bounded MRF handoff controls, including the measured negative loss."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text();exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
root=d.ROOT/'build/batch20-mrf-count-transfer';ledger=json.loads((root/'results.json').read_text());reports={}
allowed={'V32int':{'DemodDataV32','SetAdaptEcV32','SetAdaptEqV32','SetECRndTripDelayV32'},'B103prc':{'DemodDataB103'},'V17r_int':{'DemodDataV17'},'V21r_int':{'DemodDataV21'},'V27r_int':{'DemodDataV27'},'V29r_int':{'DemodDataV29'}}
for family,entry in ledger['families'].items():
 base=canonical(root/family/'baseline/candidate.o')
 for label,c in entry['cells'].items():
  q=canonical(root/family/label/'candidate.o')
  for k in ['records','objects','allocated_sizes','nontext','nontext_relocations']:assert q[k]==base[k],(family,label,k)
  assert Counter(map(str,q['text_relocations']))==Counter(map(str,base['text_relocations'])),(family,label,'texttargets')
  assert set(c.get('changed_bodies',[]))<=allowed[family],(family,label,c.get('changed_bodies'))
  assert not c.get('gains',[])
  assert c.get('losses',[])==(['SetAdaptEqV32'] if family=='V32int' and label=='int-count' else []),(family,label,c.get('losses'))
  reports[family+'/'+label]={'functions':len(c['functions']),'verdicts':c['verdicts'],'changed_bodies':c.get('changed_bodies',[]),'losses':c.get('losses',[]),'metadata_data_relocs_stable':True}
assert len(reports)==24
(root/'full-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('MRF transfer full-TU audit:24cells; stabledata/metadata/canonicalrelocs; nogains, oneSetAdaptEqV32 loss explicitly preserved; noadoption.')
