#!/usr/bin/env python3
"""Full TU/data/import audit of the four designer quiet-NaN source cells."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from gcc3_value_carriers_audit import inspect
root=d.ROOT/'build/gcc3-uref-designer-nan';family='V90ConstellationDesigner'
cells=json.loads((root/'results.json').read_text())['families'][family]['cells'];base=None;reports={}
target='_ZN24V90ConstellationDesigner23setConstellationToNoiseEfPA128_sS1_PsPhPA128_h'
assert len(cells)==4 and cells['baseline']['baseline_reproduced']
for label,c in cells.items():
 p=root/family/label/'candidate.o';q=inspect(p)
 with p.open('rb') as stream:
  elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab')
  sections={x.name:x.data() for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
  relocs=[(x.name,y['r_offset'],y['r_info_type'],tab.get_symbol(y['r_info_sym']).name) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
 if label=='baseline':base=q;bs=sections;br=relocs
 expected_drop=label=='high-1-low-1'
 allowed_records=dict(base['records'])
 if expected_drop:allowed_records.pop('nanf')
 assert q['records']==allowed_records,(label,'unaccounted symbol metadata/import change')
 assert q['nobits']==base['nobits'],(label,'BSS')
 assert relocs==br,(label,'nontext relocation change')
 assert set(sections)==set(bs),(label,'allocated section inventory')
 for name,data in sections.items():
  if name=='.rodata.cst4':
   chunks=lambda buf:Counter(buf[i:i+4].hex() for i in range(0,len(buf),4))
   expected=chunks(bs[name]);
   if label!='baseline':expected['0000c07f']+=1
   assert chunks(data)==expected,(label,'constant pool changes beyond one SF quiet NaN')
  elif name=='.rodata.str1.1':
   before=Counter(bs[name].split(b'\0'));after=Counter(data.split(b'\0'))
   if expected_drop:before[b'']-=1
   assert after==before,(label,'strings beyond removed empty nanf argument')
  elif name=='.rodata.str1.4':
   assert Counter(data.split(b'\0'))==Counter(bs[name].split(b'\0')),(label,'diagnostic string values changed')
  else:assert data==bs[name],(label,name,'unaccounted allocated nontext bytes')
 assert not c.get('gains',[]) and not c.get('losses',[]),label
 bystanders={}
 for fn in c['functions']:
  if fn==target:continue
  a,ar=d.b.body(root/family/'baseline/candidate.o',fn);b,brf=d.b.body(p,fn)
  assert d.b.verdict(a,ar,b,brf)==('EXACT',0),(label,fn,'canonical bystander')
  bystanders[fn]={'canonical':'EXACT','relocations_equal':ar==brf}
 before=Counter(repr(value) for value in d.b.body(root/family/'baseline/candidate.o',target)[1].values())
 after=Counter(repr(value) for value in d.b.body(p,target)[1].values())
 if label!='baseline':before[repr(('R_386_32',('merge-entry',4,b'\x00\x00\xc0\x7f',0)))]+=1
 if expected_drop:
  before[repr(('R_386_32',('merge-string',b'\x00'))) ]-=1
  before[repr(('R_386_PC32',('symbol','nanf',4294967292))) ]-=1
 assert +after==+before,(label,'unexpected target canonical relocation changes')
 instructions=d.b.insns(p,target)
 nan_calls=sum(mn=='call' and "'nanf'" in ops for mn,ops in instructions)
 assert nan_calls==(0 if expected_drop else 1),(label,'nanf call census')
 reports[label]={'functions':len(c['functions']),'canonical_bystanders':len(bystanders),
  'named_data':sum(row[0]=='STT_OBJECT' for row in q['records'].values()),'bystanders':bystanders,'nanf_calls':nan_calls,
  'target_size':len(d.b.body(p,target)[0]),'target_verdict':c['verdicts'][target],
  'quiet_nan_constants_added':0 if label=='baseline' else 1,
  'diagnostic_string_pool_reordered':sections['.rodata.str1.4']!=bs['.rodata.str1.4'],
  'empty_string_removed':expected_drop,'imports_removed':['nanf'] if expected_drop else [],
  'raw_changed_bodies':c.get('changed_bodies',[])}
(root/'complete-tu-audit.json').write_text(json.dumps({'valid_full_tus':4,'reports':reports},indent=2)+'\n')
print('4/4 full-TU audits;23 canonical bystanders unchanged each;one SF quiet NaN added;both-NAN removes nanf/empty argument;zero exact gains/losses')
