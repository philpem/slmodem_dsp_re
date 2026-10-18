#!/usr/bin/env python3
"""Audit complete SGD candidate objects, including all non-code payload."""
import json
import tempfile
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

def inspect(path):
 with open(path,'rb') as f:
  elf=ELFFile(f); names={i:s.name for i,s in enumerate(elf.iter_sections())}
  symbols={}
  for s in elf.get_section_by_name('.symtab').iter_symbols():
   if not s.name:continue
   e=s.entry;ind=e['st_shndx']
   symbols[s.name]=[e['st_info']['type'],e['st_info']['bind'],e['st_other']['visibility'],names.get(ind,ind),None if e['st_info']['type']=='STT_FUNC' else e['st_size']]
  sections={s.name:[s['sh_type'],s['sh_flags'],s['sh_addralign'],s['sh_entsize'],s.data().hex()] for s in elf.iter_sections() if s['sh_flags']&2 and s.name!='.text'}
  relocations={}
  symtab=elf.get_section_by_name('.symtab')
  for sec in elf.iter_sections():
   if not isinstance(sec,RelocationSection) or names[sec['sh_info']]=='.text':continue
   relocations[sec.name]=[(r['r_offset'],r['r_info_type'],symtab.get_symbol(r['r_info_sym']).name) for r in sec.iter_relocations()]
  return {'symbols':symbols,'nontext':sections,'nontext_relocations':relocations}

out={};count=0
for family in ('sgd-countdown293','sgd-tail293'):
 root=d.ROOT/'build'/family/'Sgd';base=inspect(root/'baseline/candidate.o');reports={}
 # Negative control changes one actual allocated nontext byte.
 with open(root/'baseline/candidate.o','rb') as f:
  elf=ELFFile(f)
  section=next(s for s in elf.iter_sections() if s['sh_flags']&2 and s.name!='.text' and s['sh_size'] and s['sh_type']!='SHT_NOBITS')
  offset=section['sh_offset']
 raw=bytearray((root/'baseline/candidate.o').read_bytes());raw[offset]^=1
 with tempfile.TemporaryDirectory() as tmp:
  probe=Path(tmp)/'nontext-corrupt.o';probe.write_bytes(raw)
  assert inspect(probe)!=base,'negative control failed to fire'
 ledger=json.loads((root.parent/'results.json').read_text())['families']['Sgd']['cells']
 exact={n for n,v in ledger['baseline']['verdicts'].items() if v[0]=='EXACT'}
 assert len(exact)==3
 for label,entry in ledger.items():
  assert len(entry['verdicts'])==9
  assert {n for n,v in entry['verdicts'].items() if v[0]=='EXACT'}==exact
  assert set(entry.get('changed_bodies',[])) <= {'SGD_correlate'}
 for p in sorted(root.glob('*/candidate.o')):
  v=inspect(p);assert v==base, (p,'metadata/nontext drift');reports[p.parent.name]=v;count+=1
 out[family]=reports
(d.ROOT/'build/sgd293-full-tu-audit.json').write_text(json.dumps(out,indent=2)+'\n')
assert count==21,count
print('2/2 corrupted-nontext controls rejected; 21/21 complete-TU cells: symbol names/type/binding/visibility/storage/size and every allocated nontext byte/metadata/relocation unchanged; 9/9 shared function inventory in each cell; exact set 3/9 unchanged')
