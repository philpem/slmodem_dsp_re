#!/usr/bin/env python3
"""Whole-TU audits of Dialer guards/owners and Tone IIR section boundaries."""
import json,re
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
s=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
reports={};count=0
for domain,family,allow in [('dialer-abort','Dialer',{'DialerAbort'}),('dialer-create','Dialer',{'DialerCreate'}),('toneiir-sections','toneiir',{'toneiir_progress','_iir_filter_progress'}),('toneiir-shift','toneiir',{'toneiir_progress','_iir_filter_progress'})]:
 root=driver.ROOT/('build/gcc3-batch20-'+domain);cells=json.loads((root/'results.json').read_text())['families'][family]['cells'];base=None
 for label,c in cells.items():
  p=root/family/label/'candidate.o';q=inspect(p)
  with p.open('rb') as stream:
   elf=ELFFile(stream);symtab=elf.get_section_by_name('.symtab');q['nontext']={x.name:x.data().hex() for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
   q['nontext_relocations']=[(x.name,y['r_offset'],y['r_info_type'],symtab.get_symbol(y['r_info_sym']).name,elf.get_section(x['sh_info']).data()[y['r_offset']:y['r_offset']+4].hex()) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
  if label=='baseline':base=q
  assert q==base,(domain,label,'metadata/data')
  assert not c.get('gains') and not c.get('losses');assert set(c.get('changed_bodies',[]))<=allow
  modes={}
  if domain.startswith('toneiir'):
   rtl=(root/family/label/'toneiir.c.01.rtl').read_text()
   for name in ('toneiir_progress','_iir_filter_progress'):
    part=rtl.split(';; Function '+name,1)[1].split(';; Function ',1)[0]
    modes[name]=sorted(set(re.findall(r'reg/v:(\w+) \d+ \[ k \]',part)))
    narrow_writes=len(re.findall(r'\(set \(reg/v:SI \d+ \[ k \]\)\s+\(sign_extend:SI \(subreg:HI',part))
    expected=(8 if ('expanded-sections-1' in label or 'shift-capture' in label) else 2) if ('short-taps-1' in label or 'shift-capture' in label) else 0
    assert narrow_writes==expected,(label,name,narrow_writes,expected)
    modes[name]={'register_modes':modes[name],'low_word_sign_extensions':narrow_writes}
  if domain=='toneiir-shift':
   part=rtl.split(';; Function _iir_filter_progress',1)[1].split(';; Function ',1)[0];assert ('[ shift ]' in part)==('shift-capture' in label)
  reports[domain+'/'+label]={'verdicts':c['verdicts'],'audit':q,'tap_modes':modes};count+=1
assert count==12
(driver.ROOT/'build/batch20-call-dialer-audit.json').write_text(json.dumps(reports,indent=2,default=lambda x: x.hex() if isinstance(x,bytes) else str(x))+'\n')
print('12/12 complete TU metadata/data/nontext/relocation audits pass; Tone IIR narrow tap write detectors fire (registers remain promoted SI); no exact gains/losses')
