#!/usr/bin/env python3
"""Audit full-TU and early/allocated RTL controls for the B103 transmitter lifetime and width domains."""
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
runs={'reload':('baseline','owner-reload'),'width':('baseline','implicit-count','unsigned-result','unsigned-result-implicit-count','reload','reload-implicit-count','reload-unsigned-result','reload-unsigned-result-implicit-count')}
targets=['ModDataB103','TxNoCarrierB103'];total=0
for run,cells in runs.items():
 root=driver.ROOT/('build/playbook-b103-tx-'+run);folder=root/'B103prc';reports={cell:fullinspect(folder/cell/'candidate.o') for cell in cells}
 base=reports['baseline'];functions=sorted(b.sizes(str(folder/'baseline/candidate.o')));assert len(functions)==17
 records=__import__('json').loads((root/'results.json').read_text())['families']['B103prc']['cells']
 for cell in cells:
  for key in ('records','objects','allocated_sizes','nontext','nontext_relocations'):assert reports[cell][key]==base[key],(run,cell,key)
  obj=str(folder/cell/'candidate.o');total+=1
  reports[cell]['changed']=records[cell].get('changed_bodies',[])
  reports[cell]['targets']={}
  assert sum(v[0]=='EXACT' for v in records[cell]['verdicts'].values())==4
  for name in targets:
   instructions=b.insns(obj,name)
   import subprocess
   disassembly=subprocess.check_output(['python3',str(driver.ROOT/'tools/dis.py'),obj,name],text=True,stderr=subprocess.DEVNULL)
   reloads=len(re.findall(r'mov\s+0x54\(%[a-z]+\),%',disassembly))
   reports[cell]['targets'][name]={'bytes':b.sizes(obj)[name],'root_loads':reloads,'signed_extensions':disassembly.count('cwtl'),'zero_extensions_ax':len(re.findall(r'movzwl\s+%ax,',disassembly))}
   owner_reload=('reload' in cell)
   assert reloads==(2 if owner_reload else 1),(cell,name,reloads)
  if 'implicit-count' in cell:
   sibling=cell.replace('-implicit-count','').replace('implicit-count','baseline')
   assert Path(obj).read_bytes()==(folder/sibling/'candidate.o').read_bytes(),(cell,sibling)
 (root/'complete-object-audit.json').write_text(json.dumps(reports,indent=2,default=lambda v:v.hex() if isinstance(v,bytes) else str(v))+'\n')
 print(run,len(cells),'cells,17 functions/3 data;metadata/nontext agree;strict4/17;distinct',len({(folder/c/'candidate.o').read_bytes() for c in cells}))
print(total,'valid complete-TU compilations reviewed')
