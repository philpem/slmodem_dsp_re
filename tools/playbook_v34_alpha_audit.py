#!/usr/bin/env python3
"""Audit full-TU and early/allocated RTL controls for the quotient-width domain."""
from pathlib import Path
import json,re,difflib
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
source=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])
root=driver.ROOT/'build/playbook-v34-alpha-width';folder=root/'V34TX';reports={}
for cell in ('baseline','short-quotient'):
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
 assert reports['baseline'][key]==reports['short-quotient'][key],key
functions=sorted(b.sizes(str(folder/'baseline/candidate.o')))
changed=[name for name in functions if b.body(str(folder/'baseline/candidate.o'),name)!=b.body(str(folder/'short-quotient/candidate.o'),name)]
assert changed==['adaptecho','updateAlpha'],changed
reports['scope']={'functions':len(functions),'named_data':len(reports['baseline']['objects']),'changed':changed}
reports['alpha']={name:b.alpha_why(b.insns(b.BLOB,name),b.insns(str(folder/'short-quotient/candidate.o'),name)) for name in changed}
for name in changed:
 print(name,'blob/current/candidate bytes',len(b.body(b.BLOB,name)[0]),len(b.body(str(folder/'baseline/candidate.o'),name)[0]),len(b.body(str(folder/'short-quotient/candidate.o'),name)[0]))
 print('alpha',reports['alpha'][name])
reports['stages']={}
for stage in ('01.rtl','20.combine','25.greg'):
 blocks={}
 for cell in ('baseline','short-quotient'):
  text=(folder/cell/('V34TX.c.'+stage)).read_text().split(';; Function updateAlpha',1)[1].split(';; Function ',1)[0]
  for token in ('var_decl','function_decl','string_cst'):
   text=re.sub(r'(<'+token+r' )0x[0-9a-f]+',r'\1HEAP',text)
  text=re.sub(r'(\(note \d+ \d+ \d+ )0x[0-9a-f]+( NOTE_INSN_BLOCK_(?:BEG|END))',r'\1HEAP\2',text)
  blocks[cell]=text
  (folder/cell/('updateAlpha.'+stage+'.normalized')).write_text(text)
 diff='\n'.join(difflib.unified_diff(blocks['baseline'].splitlines(),blocks['short-quotient'].splitlines(),n=3))+'\n'
 (root/('updateAlpha.'+stage+'.diff')).write_text(diff)
 report={'predicate_baseline_si':'compare:CCGOC (reg/v:SI 67 [ r ])' in blocks['baseline'], 'predicate_candidate_hi':'compare:CCGOC (subreg/s:HI (reg/v:SI 67 [ r ]) 0)' in blocks['short-quotient']}
 if stage in ('01.rtl','20.combine'):
  assert report['predicate_baseline_si'] and report['predicate_candidate_hi']
 if stage=='25.greg':
  # Header ends before the allocated RTL instruction stream.
  report['allocation_header_identical']=blocks['baseline'].split('\n(insn',1)[0]==blocks['short-quotient'].split('\n(insn',1)[0]
  assert report['allocation_header_identical']
 reports['stages'][stage]=report
(root/'complete-object-stage-audit.json').write_text(json.dumps(reports,indent=2,default=lambda value:value.hex() if isinstance(value,bytes) else str(value))+'\n')
print('Two cells:7 functions,',reports['scope']['named_data'],'named data; metadata/nontext/five siblings agree; allocation header identical')
