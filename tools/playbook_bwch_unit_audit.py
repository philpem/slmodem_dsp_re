#!/usr/bin/env python3
"""Audit named data owners and initial RTL in the four-cell Bw visibility replay."""
from pathlib import Path
import json
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b = driver.b
out = driver.ROOT / 'build/playbook-bwch-unit-visibility'
r = json.loads((out / 'results.json').read_text())
cells = r['families']['bwchdem']['cells']
def inspect(path):
 with path.open('rb') as stream:
  elf=ELFFile(stream);symtab=elf.get_section_by_name('.symtab');names={i:s.name for i,s in enumerate(elf.iter_sections())};objects={};records={}
  for symbol in symtab.iter_symbols():
   name=symbol.name;v=symbol.entry;index=v['st_shndx'];section=names[index] if isinstance(index,int) else index
   if name:records[name]=[v['st_info']['type'],v['st_info']['bind'],v['st_other']['visibility'],section,v['st_size'] if v['st_info']['type']!='STT_FUNC' else None]
   if v['st_info']['type']=='STT_OBJECT':
    data=bytearray(elf.get_section(index).data()[v['st_value']:v['st_value']+v['st_size']]);relocations={}
    for relsec in elf.iter_sections():
     if not isinstance(relsec,RelocationSection) or relsec['sh_info']!=index:continue
     for rel in relsec.iter_relocations():
      off=rel['r_offset']-v['st_value']
      if 0<=off<len(data):
       kind={1:'R_386_32',2:'R_386_PC32'}[rel['r_info_type']];target=symtab.get_symbol(rel['r_info_sym']).name or names[symtab.get_symbol(rel['r_info_sym'])['st_shndx']]
       relocations[off]=[kind,b.relocation_target(kind,target,bytes(data[off:off+4]),b.section_symbols(str(path)))];data[off:off+4]=b'\0'*4
    objects[name]={'section':section,'offset':v['st_value'],'bytes':data.hex(),'relocations':relocations}
  return {'records':records,'objects':objects,'allocated_sizes':{s.name:s['sh_size'] for s in elf.iter_sections() if s['sh_flags']&2 and s.name!='.text'}}
reports={n:inspect(out/'bwchdem'/n/'candidate.o') for n in cells};base=reports['baseline']
for n,q in reports.items():
 assert q['records']==base['records'];q['allocated_size_changes']={k:[base['allocated_sizes'].get(k),v] for k,v in q['allocated_sizes'].items() if base['allocated_sizes'].get(k)!=v}
 assert {k:{a:v[a] for a in ('section','bytes','relocations')} for k,v in q['objects'].items()}=={k:{a:v[a] for a in ('section','bytes','relocations')} for k,v in base['objects'].items()}
 rtl=(out/'bwchdem'/n/'bwchdem.c.01.rtl').read_text().split(';; Function BwChDem_Create',1)[1].split(';; Function ',1)[0]
 q['create_initial_table_reference']='block_size_table' in rtl
 print(n,'initial table reference',q['create_initial_table_reference'],'objects',len(q['objects']),'same values/reloc targets/size/type/binding/visibility; offsets', {k:v['offset'] for k,v in q['objects'].items()})
p=str(out/'bwchdem/deferred-no-unit/candidate.o');print('candidate grade1',b.alpha_why(b.insns(b.BLOB,'BwChDem_Create'),b.insns(p,'BwChDem_Create')))
(out/'complete-object-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
