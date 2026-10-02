#!/usr/bin/env python3
"""Audit full-TU and early/allocated RTL controls for the eager capability-predicate domain."""
from pathlib import Path
import json,re,difflib
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
source=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])
root=driver.ROOT/'build/playbook-prefilter-eager';folder=root/'V90PreFilter';reports={}
for cell in ('baseline','eager-or'):
 path=folder/cell/'candidate.o';report=inspect(path);nontext={};relocs={}
 with path.open('rb') as stream:
  elf=ELFFile(stream);syms=elf.get_section_by_name('.symtab');names={i:s.name for i,s in enumerate(elf.iter_sections())}
  for i,section in enumerate(elf.iter_sections()):
   if not section['sh_flags']&2 or section.name=='.text':continue
   raw=bytearray(section.data());canonical={}
   for relsec in elf.iter_sections():
    if not isinstance(relsec,RelocationSection) or relsec['sh_info']!=i:continue
    for relocation in relsec.iter_relocations():
     offset=relocation['r_offset'];symbol=syms.get_symbol(relocation['r_info_sym']);kind={1:'R_386_32',2:'R_386_PC32'}[relocation['r_info_type']]
     canonical[offset]=(kind,b.relocation_target(kind,symbol.name or names[symbol['st_shndx']],bytes(raw[offset:offset+4]),b.section_symbols(str(path))));raw[offset:offset+4]=b'\0'*4
   nontext[section.name]=raw.hex();relocs[section.name]=canonical
 report.update(nontext=nontext,nontext_relocations=relocs)
 reports[cell]=report
for key in ('records','objects','allocated_sizes','nontext','nontext_relocations'):
 assert reports['baseline'][key]==reports['eager-or'][key],key
functions=sorted(b.sizes(str(folder/'baseline/candidate.o')))
changed=[name for name in functions if b.body(str(folder/'baseline/candidate.o'),name)!=b.body(str(folder/'eager-or/candidate.o'),name)]
assert changed==['_ZN12V90PreFilter16getV90CapabilityEv','_ZNK12V90PreFilter13isV90WithEia6Ev'],changed
assert len(functions)==16
reports['scope']={'functions':len(functions),'named_data':len(reports['baseline']['objects']),'changed':changed}
(root/'complete-object-audit.json').write_text(json.dumps(reports,indent=2,default=lambda value:value.hex() if isinstance(value,bytes) else str(value))+'\n')
print('Two cells:',len(functions),'functions,',len(reports['baseline']['objects']),'named data; metadata/nontext/14 sibling bodies agree')
