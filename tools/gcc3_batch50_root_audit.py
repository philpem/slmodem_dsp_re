#!/usr/bin/env python3
"""Audit finite root controls, named owners, whole allocated nontext and bystanders."""
import json
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
text=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(text[text.index('def inspect'):text.index('reports=')])
text=(d.ROOT/'tools/gcc3_batch20_root_audit.py').read_text()
exec(text[text.index('def allocated'):text.index('\nknown=')])
reports={};total=0
families={'pulse-ready':{'IsPulseDialerReady','SetPulseBreakTime'},'pulse-ready-return':{'IsPulseDialerReady','SetPulseBreakTime'},'pulse-ready-break':{'IsPulseDialerReady','SetPulseBreakTime'},'pulse-digit':{'PulseDialDigit'},'fifo':{'FIFO8_read','FIFO8_write'},'voice-status':{'_handle_status','voice_modem'},'cid-owner':{'cid_get_strings'},'cid-output-lifetime':{'cid_get_strings'},'tone-kill':{'TONE_kill'},'detector-copy':{'detector_create'},'generic-tone':{'_ZN19GenericToneDetector7processEPfj'},'generic-tone-guard':{'_ZN19GenericToneDetector7processEPfj'},'generic-tone-hits':{'_ZN19GenericToneDetector7processEPfj'},'voice-register':{'vce_get_sreg'},'voice-register-lifetime':{'vce_get_sreg'},'wrapper-dispatch':{'dp_wrapper_create'},'wrapper-creation':{'dp_wrapper_create'},'wrapper-cleanup':{'dp_wrapper_create'},'cid-hex':{'data_raw','data_unformatted_output','data_formatted_output'},'cid-hex-orientation':{'data_raw','data_unformatted_output','data_formatted_output'},'cid-hex-signed-orientation':{'data_raw','data_unformatted_output','data_formatted_output'}}
for family,allowed in families.items():
 root=d.ROOT/'build'/('gcc3-batch50-'+family)
 ledger=json.loads((root/'results.json').read_text())
 for name,data in ledger['families'].items():
  baseline=root/name/'baseline/candidate.o';meta=inspect(baseline);nontext=allocated(baseline)
  for label,entry in data['cells'].items():
   path=root/name/label/'candidate.o';q=inspect(path);a=allocated(path)
   removed=set(meta['objects'])-set(q['objects'])
   if removed:
    assert family.startswith('wrapper-') and removed=={'dpw_rate_pairs'},(family,label,removed)
    assert q['records']=={k:v for k,v in meta['records'].items() if k not in removed},(family,label,'metadata')
    assert q['objects']=={k:v for k,v in meta['objects'].items() if k not in removed},(family,label,'named objects')
    # The only removed allocated section consists entirely of this extra table.
    assert bytes.fromhex(meta['objects']['dpw_rate_pairs']['bytes'])==bytes.fromhex(nontext['.rodata']['bytes'])
    assert {k:v for k,v in a.items() if k!='.rodata'}=={k:v for k,v in nontext.items() if k!='.rodata'}
    assert '.rodata' not in a or not bytes.fromhex(a['.rodata']['bytes'])
   else:
    assert q['records']==meta['records'],(family,label,'metadata')
    assert q['objects']==meta['objects'],(family,label,'named objects')
    assert a==nontext,(family,label,'allocated nontext')
   changed=[s for s in b.sizes(str(path)) if b.body(str(baseline),s)!=b.body(str(path),s) and b.verdict(*b.body(str(baseline),s),*b.body(str(path),s))[0]!='EXACT']
   assert set(changed)<=allowed,(family,label,changed)
   assert not entry.get('losses'),(family,label,'exact loss')
   reports[family+'/'+label]={'functions':len(entry['verdicts']),'changed':changed,'gains':entry.get('gains',[]),'named_data':len(q['objects'])};total+=1
assert total==91,total
checks=[('pulse-digit','call','owner-1-count-1','PulseDialDigit',146),('voice-status','voice','if-assignment','_handle_status',44),('cid-output-lifetime','cid','late-1-direct-1','cid_get_strings',145),('voice-register-lifetime','voice','common-guarded-after-call','vce_get_sreg',188),('cid-hex-signed-orientation','Data','late-signed-1-decimalfirst-1','data_raw',117),('cid-hex-signed-orientation','Data','late-signed-1-decimalfirst-1','data_unformatted_output',117)]
for family,name,label,symbol,size in checks:
 p=d.ROOT/'build'/('gcc3-batch50-'+family)/name/label/'candidate.o'
 assert b.sizes(str(p))[symbol]==size
 assert b.verdict(*b.body(b.BLOB,symbol),*b.body(str(p),symbol))==('EXACT',0)
(d.ROOT/'build/batch50-root-full-tu-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('91/91 complete TUs audited; only witnessed extra rate table removed in declined controls; changes bounded, no exact losses')
print('Six positive controls fire: pulse146B, status44B, S-register188B, CID strings145B, raw117B and unformatted117B exact; all declined cells preserved')
