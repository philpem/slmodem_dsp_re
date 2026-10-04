#!/usr/bin/env python3
"""Audit final finite V22 and integrated B103 banner controls."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text();exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
with open(b.BLOB,'rb') as f:
 blob=ELFFile(f);strings={x for s in blob.iter_sections() if s.name.startswith('.rodata.str') for x in s.data().split(b'\0') if x}
reports={};count=0
allowed={'B103prc':{'B103AnswerNextState','B103OriginateNextState','B103FP_create','ModDataB103','TxNoCarrierB103','TxHdxDataB103','TxHdxMarksB103','TxHdxSilenceB103'},'V22int':{'DemodDataV22'},'v22_mrf':set(),'v22_fse':{'V22_FSE_init'}}
for rn in ['batch20-b103-integrated-banner','batch20-v22-mrf-formal','batch20-v22-fse-copy']:
 root=d.ROOT/'build'/rn;ledger=json.loads((root/'results.json').read_text())
 for family,entry in ledger['families'].items():
  base=canonical(root/family/'baseline/candidate.o')
  for label,c in entry['cells'].items():
   q=canonical(root/family/label/'candidate.o');debug=family=='B103prc' and label!='baseline'
   assert q['records']==base['records'],(rn,family,label,'metadata')
   assert q['objects']==base['objects'],(rn,family,label,'named data')
   for key in ['allocated_sizes','nontext','nontext_relocations']:
    assert {s:v for s,v in q[key].items() if not(debug and s.startswith('.rodata.str'))}=={s:v for s,v in base[key].items() if not(debug and s.startswith('.rodata.str'))},(rn,family,label,key)
   added={x for sec,raw in q['nontext'].items() if sec.startswith('.rodata.str') for x in bytes.fromhex(raw).split(b'\0') if x}-{x for sec,raw in base['nontext'].items() if sec.startswith('.rodata.str') for x in bytes.fromhex(raw).split(b'\0') if x}
   assert added<=strings,(rn,family,label,added-strings)
   additions=Counter(map(str,q['text_relocations']))-Counter(map(str,base['text_relocations']));removals=Counter(map(str,base['text_relocations']))-Counter(map(str,q['text_relocations']))
   assert not removals,(rn,family,label,removals)
   if debug:assert all(any(t in x for t in ['dsplibs_debug_level','dsplibs_debug_printf','B103_STATE','default','B103FP version','Sep 22 2005','15:48:11']) for x in additions),(rn,family,label,additions)
   else:assert not additions,(rn,family,label,additions)
   assert set(c.get('changed_bodies',[]))<=allowed[family],(rn,family,label,c.get('changed_bodies'))
   assert not c.get('losses',[]),(rn,family,label,c.get('losses'))
   if family!='B103prc':assert not c.get('gains',[]),(rn,family,label,c.get('gains'))
   else:assert set(c.get('gains',[]))==({'B103OriginateNextState','ModDataB103','TxNoCarrierB103'} if debug else set())
   reports[rn+'/'+family+'/'+label]={'functions':len(c['functions']),'verdicts':c['verdicts'],'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'losses':c.get('losses',[]),'raw_baseline_merge':c['object_hash']==entry['cells']['baseline']['object_hash']};count+=1
assert count==13,count
(d.ROOT/'build/batch20-last-v22-b103-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('FinalV22/B103full-TU audit13cells; metadata/data/nontext/canonicalrelocs stable exceptoriginalB103debugimports; noadditionalgains/losses; allMRFhelper cellsrawmerge.')
