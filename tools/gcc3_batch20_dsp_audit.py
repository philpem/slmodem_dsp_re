#!/usr/bin/env python3
"""Complete-TU and causal RTL audit of the declared small DSP controls."""
import json,re
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
s=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
reports={};total=0;patterns={}
for suffix in ['dsp','dsp-followup','fsm','dotp','mtd','fsm-followup']:
 root=driver.ROOT/'build'/('gcc3-batch20-'+suffix)
 r=json.loads((root/'results.json').read_text())
 for family,fr in r['families'].items():
  baseline=root/family/'baseline/candidate.o';base=inspect(baseline)
  with baseline.open('rb') as stream:
   elf=ELFFile(stream);nontext={x.name:x.data().hex() for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
  for label,c in fr['cells'].items():
   path=root/family/label/'candidate.o';assert inspect(path)==base,(suffix,family,label,'metadata/data')
   with path.open('rb') as stream:
    elf=ELFFile(stream);assert nontext=={x.name:x.data().hex() for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
    symtab=elf.get_section_by_name('.symtab')
    extra=[(x.name,y['r_offset'],y['r_info_type'],symtab.get_symbol(y['r_info_sym']).name,elf.get_section(x['sh_info']).data()[y['r_offset']:y['r_offset']+4].hex()) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
    if label=='baseline':baseline_extra=extra
    assert extra==baseline_extra,(suffix,family,label,'nontext relocation')
   assert not c.get('losses',[]),(suffix,family,label,c['losses'])
   allowed={'fpm_iir':{'FPM_iir_filt','FPM_iir_filt_block','FPM_iir_filt_II'},'fpm_adeq':{'FPM_lmsupd','FPM_lmsupd2','FPM_block_update'},'fpm_fsm':{'FPM_FSM_modulate'},'fpm_div32':{'FPM_circ_dotp2'},'fpm_mtd':{'FPM_MTD_detect'}}[family]
   assert set(c.get('changed_bodies',[]))<=allowed
   reports[suffix+'/'+family+'/'+label]={'functions':len(c['functions']),'data':len(base['objects']),'changes':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'source_hash':c['source_hash'],'object_hash':c['object_hash']}
   total+=1
 # preserve per-function stage records, including controls that fail to hit.
 if suffix in ['dsp','dsp-followup']:
  for family,fr in r['families'].items():
   for label in fr['cells']:
    for name in ['FPM_lmsupd','FPM_lmsupd2'] if family=='fpm_adeq' else ['FPM_iir_filt']:
     for stage in ['01.rtl','20.combine','22.regmove','25.greg','33.sched2']:
      text=(root/family/label/(family+'.c.'+stage)).read_text()
      found=[x for x in re.split(r'^;; Function ',text,flags=re.M)[1:] if x.splitlines()[0].strip()==name]
      assert len(found)==1
      patterns[suffix+'/'+label+'/'+name+'/'+stage]=instructions(';; Function '+found[0])
assert total==72,total
winner=driver.ROOT/'build/gcc3-batch20-dsp-followup/fpm_adeq/lms2-late-1-short-0/candidate.o'
for name in ['FPM_lmsupd','FPM_lmsupd2']:assert b.verdict(*b.body(b.BLOB,name),*b.body(str(winner),name))==('EXACT',0)
assert b.body(str(winner),'FPM_block_update')==b.body(str(driver.ROOT/'build/gcc3-batch20-dsp/fpm_adeq/baseline/candidate.o'),'FPM_block_update')
# Across source cells, isolate the pointer/source-load boundaries before allocation.
for name in ['FPM_lmsupd','FPM_lmsupd2']:
 base=patterns['dsp/baseline/'+name+'/20.combine']
 crossed=patterns['dsp/lms-index-1-dest-1/'+name+'/20.combine']
 assert base!=crossed
for stage in ['01.rtl','20.combine']:
 early=patterns['dsp-followup/lms2-late-0-short-0/FPM_lmsupd2/'+stage]
 late=patterns['dsp-followup/lms2-late-1-short-0/FPM_lmsupd2/'+stage]
 assert early!=late
for label,reverse in [('lms2-late-0-short-0', False),('lms2-late-1-short-0', True)]:
 p=patterns['dsp-followup/'+label+'/FPM_lmsupd2/01.rtl']
 t=[int(uid) for uid,x in p.items() if x[0]=='set' and 't' in x[1]]
 dest=[int(uid) for uid,x in p.items() if x[0]=='set' and 'dest' in x[1]]
 assert len(t)==len(dest)==2 and all((u<v)==reverse for u,v in zip(t,dest)),(label,t,dest)
report={'valid_complete_tus':total,'stage_records':len(patterns),'source_hashes':len({v['source_hash'] for v in reports.values()}),'object_hashes':len({v['object_hash'] for v in reports.values()}),'causal_controls':6,'reports':reports}
(driver.ROOT/'build/batch20-dsp-audit.json').write_text(json.dumps(report,indent=2)+'\n')
(driver.ROOT/'build/batch20-dsp-stage-patterns.json').write_text(json.dumps(patterns,indent=2)+'\n')
print(f'{total}/{total} complete TUs preserve metadata/data/nontext/relocation targets; no exact losses; {len(patterns)} stage records; 6/6 boundary controls')
