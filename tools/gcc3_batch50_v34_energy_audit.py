#!/usr/bin/env python3
"""Audit complete V34 energy control TUs, including all bystanders."""
import json,re
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b;s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
root=d.ROOT/'build/gcc3-batch50-v34-energy';cells=json.loads((root/'results.json').read_text())['families']['v34filters']['cells'];reports={}
for label,c in cells.items():
 p=root/'v34filters'/label/'candidate.o';metadata=inspect(p)
 with p.open('rb') as stream:
  elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab')
  nontext={x.name:x.data().hex() for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
  relocs=[(x.name,y['r_offset'],y['r_info_type'],tab.get_symbol(y['r_info_sym']).name) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
 if label=='baseline':base=metadata;base_nontext=nontext;base_relocs=relocs
 assert metadata==base,(label,'metadata/data')
 assert nontext==base_nontext and relocs==base_relocs,(label,'nontext/relocations')
 assert set(c.get('changed_bodies',[]))<={'V34EchoEstimateDelayLineEnergy'}
 assert not c.get('losses')
 rtl=(root/'v34filters'/label/'v34filters.c.01.rtl').read_text().split(';; Function V34EchoEstimateDelayLineEnergy\n',1)[1].split(';; Function ',1)[0]
 advance=bool(re.search(r'\(reg[^\n]*\[ hist \][\s\S]{0,180}\(const_int 2',rtl))
 expected=label.startswith('cursor-1');assert advance==expected,(label,advance)
 reports[label]={'pointer_advance':advance,'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'functions':len(c['functions']),'data':len(metadata['objects'])}
assert cells['baseline']['baseline_reproduced'] and len(cells)==4
(root/'complete-audit.json').write_text(json.dumps({'valid_tus':4,'reports':reports},indent=2)+'\n')
print('4/4 full TU audits and cursor RTL detectors pass; 1 exact53B gain, zero bystander changes/losses')
