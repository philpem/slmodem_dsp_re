#!/usr/bin/env python3
"""Full-TU audit of bounded consistent MRF ABI and consumer controls."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text()
exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
allowed={'B103prc':{'ModDataB103','TxNoCarrierB103','DemodDataB103','TxHdxDataB103','TxHdxMarksB103','TxHdxSilenceB103','B103OriginateNextState','B103AnswerNextState'},'v23rx':{'v23FP_rx_create'},'V21t_int':{'ModDataV21','TxNoCarrierV21'},'fpm_mrf':set()}
reports={};count=0
for rootname in ['batch20-mrf-formal','batch20-mrf-consumers-validated','batch20-v21-mrf']:
 root=d.ROOT/'build'/rootname;ledger=json.loads((root/'results.json').read_text())
 for family,entry in ledger['families'].items():
  baseline=canonical(root/family/'baseline/candidate.o')
  for label,c in entry['cells'].items():
   q=canonical(root/family/label/'candidate.o');diagnostics=rootname=='batch20-mrf-consumers-validated' and label=='combined' and family in ['B103prc','v23rx']
   excluded={'dsplibs_debug_level','dsplibs_debug_printf'} if diagnostics else set()
   assert {k:v for k,v in q['records'].items() if k not in excluded}=={k:v for k,v in baseline['records'].items() if k not in excluded},(rootname,family,label,'metadata')
   assert q['objects']==baseline['objects'],(rootname,family,label,'named data')
   for key in ['nontext','nontext_relocations','allocated_sizes']:
    assert {k:v for k,v in q[key].items() if not(diagnostics and k.startswith('.rodata.str'))}=={k:v for k,v in baseline[key].items() if not(diagnostics and k.startswith('.rodata.str'))},(rootname,family,label,key)
   assert set(c.get('changed_bodies',[]))<=allowed.get(family,set()),(rootname,family,label,c.get('changed_bodies'))
   assert not c.get('losses',[]),(rootname,family,label,c.get('losses'))
   reports[rootname+'/'+family+'/'+label]={'functions':len(c['functions']),'data':len(q['objects']),'changed_bodies':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'losses':c.get('losses',[]),'raw_baseline_merge':c['object_hash']==entry['cells']['baseline']['object_hash']};count+=1
assert count==82,count
(d.ROOT/'build/batch20-mrf-full-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('MRF full-TU audit:',count,'cells; stable metadata, named values/relocations, nontext; changes bounded; no baseline exact loss (diagnostics-only Answer loss retained separately).')
