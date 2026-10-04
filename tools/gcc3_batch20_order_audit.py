#!/usr/bin/env python3
"""Audit anchored source-order controls, canonical constants and table targets."""
import json
from collections import Counter
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b;s=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
root=driver.ROOT/'build/gcc3-batch20-order';ledger=json.loads((root/'results.json').read_text());reports={};checks=0
for family,fr in ledger['families'].items():
 base=None
 for label,c in fr['cells'].items():
  path=root/family/label/'candidate.o';report=inspect(path);report['nontext']={};report['nontext_relocations']={}
  with path.open('rb') as stream:
   elf=ELFFile(stream);syms=elf.get_section_by_name('.symtab');names={i:s.name for i,s in enumerate(elf.iter_sections())};symbols=list(syms.iter_symbols())
   report['function_order']=[s.name for s in sorted(symbols,key=lambda s:(s['st_value'],s.name)) if s['st_info']['type']=='STT_FUNC' and s['st_shndx']==elf.get_section_index('.text')]
   for index,section in enumerate(elf.iter_sections()):
    if not section['sh_flags']&2 or section.name=='.text':continue
    raw=bytearray(section.data());targets={}
    for relsec in elf.iter_sections():
     if not isinstance(relsec,RelocationSection) or relsec['sh_info']!=index:continue
     for rel in relsec.iter_relocations():
      off=rel['r_offset'];sym=syms.get_symbol(rel['r_info_sym']);kind={1:'R_386_32',2:'R_386_PC32'}[rel['r_info_type']];target=b.relocation_target(kind,sym.name or names[sym['st_shndx']],bytes(raw[off:off+4]),b.section_symbols(str(path)))
      if target[:2]==('section','.text'):
       address=target[2];owners=[s for s in symbols if s['st_info']['type']=='STT_FUNC' and s['st_shndx']==elf.get_section_index('.text') and s['st_value']<=address<s['st_value']+s['st_size']]
       assert len(owners)==1
       target=('function',owners[0].name,address-owners[0]['st_value'])
      targets[off]=(kind,target);raw[off:off+4]=b'\0'*4
    if section['sh_flags']&16:
     width=section['sh_entsize'];parts=raw.split(b'\0') if section['sh_flags']&32 else [bytes(raw[i:i+width]) for i in range(0,len(raw),width)]
     report['nontext'][section.name]=sorted((x.hex(),count) for x,count in Counter(bytes(x) for x in parts).items())
    else:report['nontext'][section.name]=raw.hex()
    report['nontext_relocations'][section.name]=targets
  if label=='baseline':base=report
  for key in ('records','objects','allocated_sizes','nontext'):assert report[key]==base[key],(family,label,key)
  report['canonical_table_target_changes']={key:[base['nontext_relocations'][key],v] for key,v in report['nontext_relocations'].items() if v!=base['nontext_relocations'][key]}
  report['changed_bodies']=c.get('changed_bodies',[])
  assert not c.get('gains') and not c.get('losses')
  reports[family+'/'+label]=report;checks+=1
 assert reports[family+'/blob-order']['function_order']!=reports[family+'/baseline']['function_order'],(family,'emission-order detector inert')
assert checks==4
assert not reports['V90SdDetector/blob-order']['changed_bodies']
(root/'complete-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('4/4 metadata/data/merge-constant/nontext audits; 2/2 emitted-order detectors fire; no gain/loss; SdDetector bodies invariant')
print('v34diag table target changes:',len(reports['v34diag/blob-order']['canonical_table_target_changes']))
for key,v in reports.items():print(key,'order',v['function_order'])
