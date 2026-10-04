#!/usr/bin/env python3
"""Full-TU audit of closed SSF cache and V92 fraction mode controls."""
import json,re
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
reports={};tus=0
for domain,family,filename in [('ssf-coefficient','V90SpectralShapingFilter','V90SpectralShapingFilter.cpp'),('v92-fraction','V92MappingParamsInt','V92MappingParamsInt.cpp'),('verifier-native','V90SpectralVerifier','V90SpectralVerifier.cpp')]:
 root=driver.ROOT/('build/gcc3-batch20-'+domain);cells=json.loads((root/'results.json').read_text())['families'][family]['cells'];base=None
 for label,c in cells.items():
  p=root/family/label/'candidate.o';q=inspect(p)
  with p.open('rb') as stream:
   elf=ELFFile(stream);symtab=elf.get_section_by_name('.symtab');q['nontext']={x.name:x.data().hex() for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
   q['nontext_relocations']=[(x.name,y['r_offset'],y['r_info_type'],symtab.get_symbol(y['r_info_sym']).name,elf.get_section(x['sh_info']).data()[y['r_offset']:y['r_offset']+4].hex()) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
  if label=='baseline':base=q
  assert q==base,(domain,label,'metadata/data')
  assert not c.get('gains') and not c.get('losses')
  changed=set(c.get('changed_bodies',[]))
  allowed={'V92setParamsInfoFromCPUnPck'} if domain=='v92-fraction' else {'_ZN24V90SpectralShapingFilter8progressEPKs','_ZNK24V90SpectralShapingFilter9getMetricEPKsj'}
  if domain=='verifier-native':allowed={'_ZN19V90SpectralVerifier30checkSpecialSpectralConditionsEv'}
  assert changed<=allowed
  rtl=(root/family/label/(filename+'.01.rtl')).read_text();modes={}
  if domain=='ssf-coefficient':
   for name in ('a0','a1','a2','a3','b0','b1','b2','b3'):
    modes[name]=sorted(set(re.findall(r'reg/v:(\w+) \d+ \[ '+name+r' \]',rtl)))
    assert modes[name]==(['SF'] if 'float-coeff-1' in label else ['XF']),modes
  if domain=='verifier-native':
   divisions=[x for x in driver.b.insns(str(p),'_ZN19V90SpectralVerifier30checkSpecialSpectralConditionsEv') if x[0].startswith('fdiv')];assert len(divisions)==3,divisions
  reports[domain+'/'+label]={'audit':q,'verdicts':c['verdicts'],'changed_bodies':sorted(changed),'cached_coefficient_modes':modes};tus+=1
assert tus==8
(driver.ROOT/'build/batch20-v90-precision-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('8/8 metadata/data/nontext/relocation audits pass; SSF cache mode controls fire XF -> SF; exact sets unchanged, no source adoption')
