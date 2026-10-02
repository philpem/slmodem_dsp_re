#!/usr/bin/env python3
"""Audit full-TU and early/allocated RTL controls for the Boolean-result and capture domains."""
from pathlib import Path
import json,re,difflib
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
source=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])
def fullinspect(path):
 report=inspect(path);nontext={};relocs={}
 with path.open('rb') as stream:
  elf=ELFFile(stream);syms=elf.get_section_by_name('.symtab');names={i:s.name for i,s in enumerate(elf.iter_sections())}
  for i,section in enumerate(elf.iter_sections()):
   if not section['sh_flags']&2 or section.name=='.text':continue
   raw=bytearray(section.data());canonical={}
   for relsec in elf.iter_sections():
    if not isinstance(relsec,RelocationSection) or relsec['sh_info']!=i:continue
    for relocation in relsec.iter_relocations():
     offset=relocation['r_offset'];symbol=syms.get_symbol(relocation['r_info_sym']);kind={1:'R_386_32',2:'R_386_PC32'}[relocation['r_info_type']]
     target=b.relocation_target(kind,symbol.name or names[symbol['st_shndx']],bytes(raw[offset:offset+4]),b.section_symbols(str(path)))
     if kind=='R_386_32' and target[:2]==('section','.text'):
      address=target[2]
      owners=[(sym.name,address-sym['st_value']) for sym in syms.iter_symbols() if sym['st_info']['type']=='STT_FUNC' and isinstance(sym['st_shndx'],int) and names[sym['st_shndx']]=='.text' and sym['st_value']<=address<sym['st_value']+sym['st_size']]
      assert len(owners)==1,(path,address,owners)
      import subprocess
      disassembly=subprocess.check_output(['objdump','-d',str(path)],text=True)
      starts={int(match.group(1),16) for match in re.finditer(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )+',disassembly,re.M)}
      assert address in starts,(path,address)
      target=('review-function-interior',)+owners[0]
     canonical[offset]=(kind,target);raw[offset:offset+4]=b'\0'*4
   nontext[section.name]=raw.hex();relocs[section.name]=canonical
 report.update(nontext=nontext,nontext_relocations=relocs)
 return report
runs={'bool':('baseline','eager-or','bool-logical','bool-eager'),'operand':('baseline','predicate-capture'),'value':('baseline','value-capture')}
expected={'baseline':[(0,0),(0,0),(0,0)],'eager-or':[(1,0),(1,0),(1,0)],'bool-logical':[(0,0),(0,0),(0,0)],'bool-eager':[(0,1),(0,1),(0,1)],'predicate-capture':[(0,0),(0,0),(0,0)]}
objects=set();total=0
for run,cells in runs.items():
 root=driver.ROOT/('build/playbook-prefilter-'+run);folder=root/'V90PreFilter'
 reports={cell:fullinspect(folder/cell/'candidate.o') for cell in cells}
 base=reports['baseline'];functions=sorted(b.sizes(str(folder/'baseline/candidate.o')))
 assert len(functions)==16
 for cell in cells:
  for key in ('records','objects','allocated_sizes','nontext','nontext_relocations'):assert reports[cell][key]==base[key],(run,cell,key)
  obj=str(folder/cell/'candidate.o');objects.add(Path(obj).read_bytes());total+=1
  changed=[n for n in functions if b.body(obj,n)!=b.body(str(folder/'baseline/candidate.o'),n)]
  assert changed==([] if cell=='baseline' else ['_ZN12V90PreFilter16getV90CapabilityEv','_ZNK12V90PreFilter13isV90WithEia6Ev']),(run,cell,changed)
  assert sum(b.verdict(*b.body(b.BLOB,n),*b.body(obj,n))[0]=='EXACT' for n in functions)==5
  reports[cell]['changed']=changed;reports[cell]['stages']={}
  counts=[]
  for stage in ('01.rtl','20.combine','25.greg'):
   text=(folder/cell/('V90PreFilter.cpp.'+stage)).read_text().split(';; Function int V90PreFilter::isV90WithEia6() const',1)[1].split(';; Function ',1)[0]
   if stage=='25.greg':text=text[text.index('\n(insn'):]
   counts.append((text.count('(ior:SI '),text.count('(ior:QI ')))
   reports[cell]['stages'][stage]={'or_si':counts[-1][0],'or_qi':counts[-1][1]}
  if cell in expected:assert counts==expected[cell],(run,cell,counts)
  else:print(cell,'stage SI/QI OR counts',counts)
 (root/'complete-object-stage-audit.json').write_text(json.dumps(reports,indent=2,default=lambda v:v.hex() if isinstance(v,bytes) else str(v))+'\n')
 print(run,len(cells),'cells:16 functions/0 data;14 siblings/metadata/nontext agree;strict5/16')
print(total,'valid emissions;',len(objects),'distinct raw objects')
