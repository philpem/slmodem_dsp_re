#!/usr/bin/env python3
"""Audit all full integer refinement TUs, including nonexact bystanders."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b;s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
specs={'v34-energy':('v34filters',4,{'V34EchoEstimateDelayLineEnergy'}),'v34-echo':('v34filters',5,{'V34EchoEstimateDelayLineEnergy','V34EchoFilter','V34EchoAdapt'}),'v34-report':('v34filters',9,{'V34EchoEstimateDelayLineEnergy','V34EchoReportCoeff'}),'v8-flip':('V8global',2,{'charFlip'}),'v8-dft-update':('V8Dftc',8,{'v8_dftupdate'}),'v34-report-found':('v34filters',3,{'V34EchoEstimateDelayLineEnergy','V34EchoReportCoeff'}),'v34-report-loops':('v34filters',5,{'V34EchoEstimateDelayLineEnergy','V34EchoReportCoeff'}),'v34-report-guarded':('v34filters',3,{'V34EchoEstimateDelayLineEnergy','V34EchoReportCoeff'}),'atan-inputs':('fpm_atan',3,{'FPM_atan'}),'v8-init':('V8global',9,{'charFlip','v8_rxinit','v8_txinit'}),'v8-notch':('V8Detector',2,{'notch_filter'}),'tone-reversal':('fpm_tone',4,{'FPM_TONE_find_rev'}),'v34-dft-shift':('DFTC',2,{'dftenergy'}),'sdm-compound':('fpm_sdm',2,{'FPM_SDM_descrambler'}),'sdm-postincrement':('fpm_sdm',4,{'FPM_SDM_descrambler'}),'sdm-split':('fpm_sdm',4,{'FPM_SDM_descrambler'}),'sdm-config':('fpm_sdm',3,{'FPM_SDM_descrambler'}),'service-tone-owners':('TONE',4,{'TONE_detect'}),'service-tone-ring':('TONE',5,{'TONE_detect','TONE_filter'}),'service-cid-mark':('Cidmtd',4,{'CID_MTD_detect'}),'service-cid-predicates':('Cidmtd',4,{'CID_MTD_detect'}),'service-cid-order':('Cidmtd',3,{'CID_MTD_detect'}),'service-cid-fsd':('Cidfsd',4,{'CID_FSD_demodulate'}),'service-cid-cascade':('Cidmtd',3,{'CID_MTD_detect'}),'service-tone-return':('TONE',4,{'TONE_detect'}),'service-tone-return:caller':('Detector',4,{'detector_progress'}),'service-tone-generate':('TONE',5,{'TONE_generate'}),'service-tone-duration':('TONE',3,{'TONE_generate'}),'service-tone-timer':('TONE',4,{'TONE_generate'}),'service-tone-amplitude':('TONE',3,{'TONE_generate'}),'service-tone-create':('TONE',2,{'TONE_create'}),'fdsp-echo':('Fdspkrnl',8,{'EchoCanceler','FDSP_Kernel_Loop'}),'fdsp-truth-count':('Fdspkrnl',3,{'EchoCanceler','FDSP_Kernel_Loop'}),'fdsp-init':('Fdspkrnl',4,{'FDSP_Kernel_InitObj','bValidateEnergyValue'}),'fdsp-init-default':('Fdspkrnl',4,{'FDSP_Kernel_InitObj','bValidateEnergyValue'}),'fdsp-loop':('Fdspkrnl',8,{'FDSP_Kernel_Loop'})}
reports={};total=0
for key,(family,count,allowed) in specs.items():
 root=d.ROOT/('build/gcc3-batch50-'+key.split(':')[0]);cells=json.loads((root/'results.json').read_text())['families'][family]['cells'];base=None
 assert len(cells)==count and cells['baseline']['baseline_reproduced']
 for label,c in cells.items():
  p=root/family/label/'candidate.o';q=inspect(p)
  with p.open('rb') as stream:
   elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab')
   nontext={x.name:(sorted(Counter(x.data().split(b'\0')).items()) if x['sh_flags']&32 else x.data().hex()) for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
   relocs=[(x.name,y['r_offset'],y['r_info_type'],tab.get_symbol(y['r_info_sym']).name) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
  if label=='baseline':base=q;bn=nontext;br=relocs
  pool_exception = key=='service-tone-duration' and label=='result-short-phase-2'
  if pool_exception:
   assert q['records']==base['records'] and q['objects']==base['objects']
   expected=dict(base['allocated_sizes']);expected['.rodata.cst4']=8
   assert base['allocated_sizes']['.rodata.cst4']==12 and q['allocated_sizes']==expected
   expected_nontext=dict(bn);expected_nontext['.rodata.cst4']='000000c00000003e'
   assert bn['.rodata.cst4']=='000000c00000003e00000000' and nontext==expected_nontext and relocs==br
  else:
   assert q==base,(key,label,'metadata/data')
   assert nontext==bn and relocs==br,(key,label,'nontext/relocations')
  assert set(c.get('changed_bodies',[]))<=allowed,(key,label,'bystander')
  assert not c.get('losses'),(key,label,'exact loss')
  reports[key+'/'+label]={'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'verdicts':{n:c['verdicts'][n] for n in allowed if n in c['verdicts']},'declined_zero_pool_removed':pool_exception,'functions':len(c['functions']),'data':len(q['objects'])};total+=1
assert total==151
(d.ROOT/'build/batch50-integer-complete-audit.json').write_text(json.dumps({'valid_tus':total,'reports':reports},indent=2)+'\n')
print('151/151 raw-baseline-controlled full TU audits pass: named data, relocations, metadata; declined expire-first cell removes only unused zero float constant, all nonexact bystanders; zero exact losses; copy-cursor controls also change nonexact V34EchoAdapt')
