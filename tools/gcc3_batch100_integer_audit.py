#!/usr/bin/env python3
"""Complete TU audit of independently bounded integer controls."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b;s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
specs={'agc':('fpm_agc',8,{'FPM_AGC_agc'}),'agc-boolean':('fpm_agc',3,{'FPM_AGC_agc'}),'atan':('fpm_atan',8,{'FPM_atan'}),'atan-result':('fpm_atan',4,{'FPM_atan'}),'v8-stability':('V8global',4,{'checkSignalStability'}),'v8-stability-predicate':('V8global',3,{'checkSignalStability'}),'vtb-literal':('fpm_vtb',9,{'VTB_decoder','vtb_acs'}),'vtb-traceback':('fpm_vtb',7,{'VTB_decoder'}),'v8-shape':('V8global',4,{'v8_txinit'}),'v8-shape-pair':('V8global',3,{'v8_txinit'}),'fsd-word':('fpm_fsd',8,{'FPM_FSD_demodulate'}),'fsd-mask':('fpm_fsd',16,{'FPM_FSD_demodulate'}),'fsd-counter':('fpm_fsd',8,{'FPM_FSD_demodulate'}),'vtb-snapshot':('fpm_vtb',8,{'VTB_decoder'}),'vtb-extended':('fpm_vtb',12,{'VTB_decoder'}),'vtb-min':('fpm_vtb',9,{'VTB_decoder'}),'vtb-absolute':('fpm_vtb',13,{'VTB_decoder','vtb_acs'}),'pps-capture':('fpm_pps',4,{'FPM_PPS_filter'}),'pps-rail':('fpm_pps',16,{'FPM_PPS_filter','FPM_PPS_init'}),'cid-fsd':('Cidfsd',8,{'CID_FSD_demodulate'}),'dtmf-eager':('Dtmf',4,{'dtmf_test'}),'dtmf-mask':('Dtmf',5,{'dtmf_test'}),'dtmf-cursors':('Dtmf_Detector',4,{'DTMF_MTD_detect'}),'mtk-phasor':('PHASOR',8,{'MTK_phasor'}),'dtmf-bank':('Dtmf_Detector',2,{'DTMF_MTD_detect'}),'shell-correlation':('v34shell',8,{'shellDemapper'}),'shell-init-base':('v34shell',2,{'initG248'}),'shell-init':('v34shell',8,{'initG248'}),'shell-init-cfg':('v34shell',5,{'initG248'}),'iir-form1':('fpm_iir',8,{'FPM_iir_filt_II'}),'circular-dot':('fpm_div32',3,{'FPM_circ_dotp2'}),'v8-message-field':('V8Interface',2,{'V8GetMessage'}),'iir-width':('fpm_iir',9,{'FPM_iir_filt','FPM_iir_filt_block','FPM_iir_filt_II'}),'v8-message-capture':('V8Interface',5,{'V8GetMessage'}),'v8-message-owner':('V8Interface',5,{'V8GetMessage'}),'iir-sections':('fpm_iir',8,{'FPM_iir_filt','FPM_iir_filt_block','FPM_iir_filt_II'}),'iir-result':('fpm_iir',9,{'FPM_iir_filt','FPM_iir_filt_block','FPM_iir_filt_II'}),'v8-message':('V8Interface',4,{'V8GetMessage'})}
reports={};total=0
for key,(family,count,allowed) in specs.items():
 root=d.ROOT/('build/gcc3-batch100-'+key);cells=json.loads((root/'results.json').read_text())['families'][family]['cells'];base=None
 assert len(cells)==count and cells['baseline']['baseline_reproduced']
 for label,c in cells.items():
  p=root/family/label/'candidate.o';q=inspect(p)
  with p.open('rb') as stream:
   elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab')
   nontext={x.name:(sorted(Counter(x.data().split(b'\0')).items()) if x['sh_flags']&32 else x.data().hex()) for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
   relocs=[(x.name,y['r_offset'],y['r_info_type'],tab.get_symbol(y['r_info_sym']).name) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
  if label=='baseline':base=q;bn=nontext;br=relocs
  if key in ('vtb-absolute','vtb-literal') and c.get('added_functions'):
   assert c['added_functions']==['vtb_acs']; assert q['records']['vtb_acs']==['STT_FUNC','STB_LOCAL','STV_DEFAULT','.text',None]; q['records'].pop('vtb_acs')
  assert q==base,(key,label,'metadata/data')
  assert nontext==bn and relocs==br,(key,label,'nontext/relocations')
  assert set(c.get('changed_bodies',[]))<=allowed,(key,label,'bystander')
  expected_losses=['FPM_PPS_init'] if key=='pps-rail' and label.endswith('wrap-1') else []
  assert c.get('losses',[])==expected_losses,(key,label,'unaccounted exact loss')
  reports[key+'/'+label]={'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'losses':c.get('losses',[]),'verdicts':{n:c['verdicts'][n] for n in allowed if n in c['verdicts']},'functions':len(c['functions']),'data':len(q['objects'])};total+=1
(d.ROOT/'build/batch100-integer-complete-audit.json').write_text(json.dumps({'valid_tus':total,'reports':reports},indent=2)+'\n')
print('%d/%d full TU audits pass; all named data, nontext bytes/relocations, metadata and nonexact bystanders accounted; all exact losses explicitly accounted'%(total,total))
