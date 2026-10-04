#!/usr/bin/env python3
"""Audit complete four-cell mask-counter TU and initial RTL predicates."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
root=d.ROOT/'build/gcc3-mechanism-constellation';cells=json.loads((root/'results.json').read_text())['families']['V90MappingParamsInt']['cells'];reports={};base=None
assert len(cells)==4 and cells['baseline']['baseline_reproduced']
for label,c in cells.items():
 p=root/'V90MappingParamsInt'/label/'candidate.o';q=inspect(p)
 with p.open('rb') as stream:
  elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab')
  nontext={x.name:(sorted(Counter(x.data().split(b'\0')).items()) if x['sh_flags']&32 else x.data().hex()) for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
  relocs=[(x.name,y['r_offset'],y['r_info_type'],tab.get_symbol(y['r_info_sym']).name) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
 if label=='baseline':base=q;bn=nontext;br=relocs
 assert q==base and nontext==bn and relocs==br,(label,'metadata/data/nontext/relocations')
 assert set(c.get('changed_bodies',[]))<= {'getConstellationMask','getCodecConstellationMask','setV92CPpckFromParamsInfo'},(label,'bystander')
 assert not c.get('gains',[]) and not c.get('losses',[]),label
 caller='setV92CPpckFromParamsInfo'
 original=d.b.insns(root/'V90MappingParamsInt/baseline/candidate.o',caller);candidate=d.b.insns(p,caller)
 assert len(original)==len(candidate)==230
 changes=[(i,a,b) for i,(a,b) in enumerate(zip(original,candidate)) if a!=b]
 expected=[]
 if label.startswith('ordinary-1'):expected.append((95,('jle','.+368'),('jbe','.+368')))
 if label.endswith('codec-1'):expected.append((142,('jle','.+544'),('jbe','.+544')))
 assert changes==expected,(label,'inline caller changes beyond observed counter branches')
 traces={}
 for fn in ('getConstellationMask','getCodecConstellationMask'):
  text=(root/'V90MappingParamsInt'/label/'V90MappingParamsInt.cpp.01.rtl').read_text().split(';; Function void '+fn+'(',1)[1].split(';; Function ',1)[0]
  traces[fn]={n:text.count('('+n+' ') for n in ('le','leu','gt','gtu')}
 reports[label]={'functions':len(c['functions']),'named_data':len(q['objects']),'changed_bodies':c.get('changed_bodies',[]),'traces':traces}
for fn,label in [('getConstellationMask','ordinary-1-codec-0'),('getCodecConstellationMask','ordinary-0-codec-1')]:
 a=reports['baseline']['traces'][fn];b=reports[label]['traces'][fn]
 assert a!=b,(fn,'counter predicate known control inert')
(d.ROOT/'build/gcc3-mechanism-constellation-audit.json').write_text(json.dumps({'valid_full_tus':4,'known_counter_predicates':2,'reports':reports},indent=2)+'\n')
print('4/4 full-TU audits;2/2 signed/unsigned predicate controls fire; zero gains/losses; inline consumer changes exactly bounded counter branches')
