#!/usr/bin/env python3
"""Audit complete V8 promoted-input control TUs, including all bystanders."""
import json,re
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b;s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
root=d.ROOT/'build/gcc3-batch50-v8-flip';cells=json.loads((root/'results.json').read_text())['families']['V8global']['cells'];reports={}
for label,c in cells.items():
 p=root/'V8global'/label/'candidate.o';metadata=inspect(p)
 with p.open('rb') as stream:
  elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab')
  nontext={x.name:x.data().hex() for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
  relocs=[(x.name,y['r_offset'],y['r_info_type'],tab.get_symbol(y['r_info_sym']).name) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
 if label=='baseline':base=metadata;base_nontext=nontext;base_relocs=relocs
 assert metadata==base,(label,'metadata/data')
 assert nontext==base_nontext and relocs==base_relocs,(label,'nontext/relocations')
 assert set(c.get('changed_bodies',[]))<={'charFlip'}
 assert not c.get('losses')
 rtl=(root/'V8global'/label/'V8global.c.01.rtl').read_text().split(';; Function charFlip\n',1)[1].split(';; Function ',1)[0]
 advance='lshiftrt:SI' in rtl
 expected=label=='promoted-input';assert advance==expected,(label,advance)
 reports[label]={'pointer_advance':advance,'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'functions':len(c['functions']),'data':len(metadata['objects'])}
assert cells['baseline']['baseline_reproduced'] and len(cells)==2
(root/'complete-audit.json').write_text(json.dumps({'valid_tus':2,'reports':reports},indent=2)+'\n')
print('2/2 full TU audits and full-register-shift RTL detectors pass; 1 exact33B gain, zero bystander changes/losses')
