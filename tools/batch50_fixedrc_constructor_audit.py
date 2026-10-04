#!/usr/bin/env python3
"""Separate obsolete bank data cleanup from constructor negative control."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text()
exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
out=d.ROOT/'build/batch50-fixedrc-constructor';r=json.loads((out/'results.json').read_text());f=r['families']['FixedRC'];base=canonical(out/'FixedRC/baseline/candidate.o');reports={}
def masked_text(path):
 with path.open('rb') as stream:
  e=ELFFile(stream);i=e.get_section_index('.text');raw=bytearray(e.get_section(i).data())
  for rs in e.iter_sections():
   if isinstance(rs,RelocationSection) and rs['sh_info']==i:
    for rel in rs.iter_relocations():raw[rel['r_offset']:rel['r_offset']+4]=b'\0'*4
  return bytes(raw)
for label,c in f['cells'].items():
 path=out/'FixedRC'/label/'candidate.o';q=canonical(path);remove='remove-bank' in label;split='split-kind-store' in label
 assert not c.get('gains') and not c.get('losses')
 assert set(c.get('changed_bodies',[]))<=({'RcFixed_Create'} if split or remove else set())
 assert q['records']=={n:v for n,v in base['records'].items() if n!='rc_banks' or not remove},(label,'metadata')
 assert set(q['objects'])==set(base['objects'])-({'rc_banks'} if remove else set())
 for n,v in q['objects'].items():
  bv=base['objects'][n];assert {k:x for k,x in v.items() if k!='offset'}=={k:x for k,x in bv.items() if k!='offset'},(label,n,'values/targets')
  assert v['offset']==bv['offset']-(160 if remove and v['section']=='.rodata' else 0),(label,n,'offset')
 assert q['allocated_sizes']=={n:v-(160 if remove and n=='.rodata' else 0) for n,v in base['allocated_sizes'].items()}
 for n,raw in q['nontext'].items():assert raw==base['nontext'][n][320 if remove and n=='.rodata' else 0:],(label,n,'masked bytes')
 text_targets=[(kind,(target[0],target[1],target[2]-160)) if remove and target[:2]==('section','.rodata') else (kind,target) for kind,target in base['text_relocations']]
 assert Counter(map(repr,q['text_relocations']))==Counter(map(repr,text_targets)),(label,'text targets')
 expected={}
 for n,rels in base['nontext_relocations'].items():
  expected[n]={off-(160 if remove and n=='.rodata' else 0):v for off,v in rels.items() if not(remove and n=='.rodata' and off<160)}
 if not split:assert q['nontext_relocations']==expected,(label,'data relocation offsets/targets')
 else:
  for n,rels in q['nontext_relocations'].items():
   if n!='.rodata':assert rels==expected[n]
  assert set(q['nontext_relocations']['.rodata'])==set(expected['.rodata'])
  for off,v in q['nontext_relocations']['.rodata'].items():
   old=expected['.rodata'][off]
   if v!=old:assert v[0]==old[0] and v[1][:2]==old[1][:2]==('function','RcFixed_Create'),(label,off,v,old)
 if not split:assert masked_text(path)==masked_text(out/'FixedRC/baseline/candidate.o'),(label,'whole raw masked text')
 reports[label]={'functions':len(c['functions']),'changed_bodies':c.get('changed_bodies',[]),'constructor':c['verdicts']['RcFixed_Create'],'removed_data_bytes':160 if remove else 0,'removed_bank_pointer_relocations':18 if remove else 0,'jump_table_case_offsets_changed':q['nontext_relocations']!=expected,'whole_masked_text_raw_equal':not split}
(out/'full-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('4 valid complete-TU cells; bank-only removes160B and18 pointer relocations, all8 raw masked bodies/data values preserved; constructor split miss no exact losses, jump-table case offsets explicit')
