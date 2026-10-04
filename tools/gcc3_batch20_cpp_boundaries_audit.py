#!/usr/bin/env python3
"""Full objects and RTL widths for FFT recovery and ARMA closed controls."""
import json,re
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
reports={};tus=0
for domain,family in [('fft-width','fft'),('arma-output','FloatARMA')]:
 root=driver.ROOT/('build/gcc3-batch20-'+domain)
 cells=json.loads((root/'results.json').read_text())['families'][family]['cells']
 base=None
 for label,c in cells.items():
  p=root/family/label/'candidate.o';q=inspect(p)
  with p.open('rb') as stream:
   elf=ELFFile(stream);symtab=elf.get_section_by_name('.symtab')
   q['nontext']={x.name:x.data().hex() for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
   q['nontext_relocations']=[(x.name,y['r_offset'],y['r_info_type'],symtab.get_symbol(y['r_info_sym']).name,elf.get_section(x['sh_info']).data()[y['r_offset']:y['r_offset']+4].hex()) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
  if label=='baseline':base=q
  assert q==base,(domain,label,'metadata/data/relocations')
  assert not c.get('losses')
  if domain=='fft-width':
   assert set(c.get('changed_bodies',[])) <= {'_Z7realfftPfmi'}
   text=(root/family/label/'fft.cpp.01.rtl').read_text()
   widths={name:sorted(set(re.findall(r'reg/v:(\w+) \d+ \[ '+name+r' \]',text))) for name in ('h1r','h1i','h2r','h2i')}
   assert all(v==(['DF'] if label=='baseline' else ['SF']) for v in widths.values()),widths
   assert c.get('gains',[])==([] if label=='baseline' else ['_Z7realfftPfmi'])
  else:
   widths={};assert not c.get('gains')
   assert set(c.get('changed_bodies',[])) <= {'_ZN9FloatARMA7processEf','_ZN9FloatARMA7processEPKfPfj'}
  reports[domain+'/'+label]={'audit':q,'verdicts':c['verdicts'],'initial_temporary_modes':widths};tus+=1
assert tus==6
(driver.ROOT/'build/batch20-cpp-boundaries-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('6/6 complete TU metadata/data/relocation audits pass; FFT initial temporary widths fire DF -> SF; one exact505B gain; ARMA four-cell domain closed without adoption')
