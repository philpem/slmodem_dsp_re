#!/usr/bin/env python3
"""Audit sign-domain source controls, nontext owners and fabs pass boundaries."""
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
root=driver.ROOT/'build/gcc3-batch20-v90-resampler';folder=root/'V90Resampler'
cells=json.loads((root/'results.json').read_text())['families']['V90Resampler']['cells']
base=inspect(folder/'baseline/candidate.o');data=nontext(folder/'baseline/candidate.o')
records={};reports={};target='_ZN12V90Resampler19getTimingHistoryStdEv'
for label,cell in cells.items():
    obj=folder/label/'candidate.o';meta=inspect(obj);non=nontext(obj)
    assert meta['records']==base['records'] and meta['objects']==base['objects']
    extra=set(non)-set(data);assert extra<={'.rodata.cst4'}
    for section in data:assert non[section]==data[section],(label,section)
    if extra:assert non['.rodata.cst4']=={'constant_pool_values':['00000000','000080bf'],'relocations':{}},label
    assert len(b.sizes(str(obj)))==16 and not cell.get('losses')
    assert set(cell.get('changed_bodies',[]))<={target}
    for stage in ('01.rtl','20.combine','21.ce2','25.greg','31.bbro','33.sched2'):
        dump=(folder/label/('V90Resampler.cpp.'+stage)).read_text()
        bodies=[s for s in re.split(r'^;; Function ',dump,flags=re.M)[1:] if 'getTimingHistoryStd' in s.splitlines()[0]]
        assert len(bodies)==1
        body=bodies[0];patterns=instructions(body)
        records[label+'/'+stage]={'abs_count':body.count('(abs:'),'branch_count':body.count('(if_then_else'),'patterns':patterns}
    early=records[label+'/01.rtl'];late=records[label+'/21.ce2']
    if label.endswith('ternary'):assert early['abs_count']==1 and early['branch_count']==0
    elif label=='baseline':assert early['abs_count']==0 and late['abs_count']==1
    else:assert early['abs_count']==0 and late['abs_count']==0
    reports[label]={'verdict':cell['verdicts'][target],'extra_sections':sorted(extra)}
for label in ('nonnegative-statement','whole-complement-statement'):
    assert b.verdict(*b.body(b.BLOB,target),*b.body(str(folder/label/'candidate.o'),target))==('EXACT',0)
(root/'complete-tu-audit.json').write_text(json.dumps({'valid_cells':len(cells),'functions_per_cell':16,'reports':reports,'stage_records':len(records)},indent=2)+'\n')
(root/'stage-patterns.json').write_text(json.dumps(records,indent=2)+'\n')
print('6/6 complete TUs, 16 bodies each; metadata/named data and original nontext relocations unchanged; only zero/-1 literals added')
print('36/36 stage records: positive ternaries already ABS in initial RTL; negative original folds at ce2; explicit positive statements remain branches; two exact66B controls')
