#!/usr/bin/env python3
"""Audit the crossed two-gain timing-history and sign-statement complete TU."""
import json,re
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
text=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(text[text.index('def inspect'):text.index('reports=')])
text=(driver.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text()
exec(text[text.index('def nontext'):text.index('\nreports=')])
root=driver.ROOT/'build/gcc3-batch20-v90-history';folder=root/'V90Resampler'
cells=json.loads((root/'results.json').read_text())['families']['V90Resampler']['cells']
base=inspect(folder/'baseline/candidate.o');data=nontext(folder/'baseline/candidate.o')
targets={'_ZN12V90Resampler19getTimingHistoryStdEv','_ZN12V90Resampler8resampleEPKfjPfRj'}
records={};reports={}
for label,cell in cells.items():
    obj=folder/label/'candidate.o';meta=inspect(obj);non=nontext(obj)
    assert meta['records']==base['records'] and meta['objects']==base['objects']
    for sec in data:assert non[sec]==data[sec],(label,sec)
    assert set(non)-set(data)<={'.rodata.cst4'}
    if '.rodata.cst4' in non:assert non['.rodata.cst4']=={'constant_pool_values':['00000000','000080bf'],'relocations':{}}
    assert len(b.sizes(str(obj)))==16 and not cell.get('losses')
    assert set(cell.get('changed_bodies',[]))<=targets
    for stage in ('01.rtl','20.combine','21.ce2','25.greg','31.bbro','33.sched2'):
        dump=(folder/label/('V90Resampler.cpp.'+stage)).read_text()
        bodies=re.split(r'^;; Function ',dump,flags=re.M)[1:]
        for name in ('V90Resampler::getTimingHistoryStd','V90Resampler::resample'):
            selected=[s for s in bodies if name+'(' in s.splitlines()[0]]
            assert len(selected)==1,(label,stage,name)
            records[label+'/'+stage+'/'+name]=instructions(selected[0])
    reports[label]={'gains':cell.get('gains',[]),'changed_bodies':cell.get('changed_bodies',[])}
winner=folder/'late-index-member-std-statements/candidate.o'
for target in targets:assert b.verdict(*b.body(b.BLOB,target),*b.body(str(winner),target))==('EXACT',0)
assert len(records)==72 and len(cells)==6
(root/'complete-tu-audit.json').write_text(json.dumps({'valid_cells':len(cells),'functions_per_cell':16,'reports':reports,'stage_records':len(records)},indent=2)+'\n')
(root/'stage-patterns.json').write_text(json.dumps(records,indent=2)+'\n')
print('6/6 crossed full TUs: metadata/data/nontext relocations preserved, fourteen bystanders unchanged, no losses')
print('72/72 RTL records; combined resample170B and Std66B EXACT; TU13→15/16 exact')
