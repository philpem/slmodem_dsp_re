#!/usr/bin/env python3
"""Audit all evaluator source boundaries, byte-exact gains and initial reload evidence."""
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
root=driver.ROOT/'build/gcc3-batch20-v90-connection';folder=root/'V90ConnectionEvaluator'
cells=json.loads((root/'results.json').read_text())['families']['V90ConnectionEvaluator']['cells']
base=inspect(folder/'baseline/candidate.o');data=nontext(folder/'baseline/candidate.o')
targets={'_ZN22V90ConnectionEvaluator20indicateLocalRetrainEv','_ZN22V90ConnectionEvaluator21indicateRemoteRetrainEv','_ZN22V90ConnectionEvaluator14updateAvePdsnrEfj'}
records={};reports={}
for label,cell in cells.items():
    obj=folder/label/'candidate.o'
    assert inspect(obj)==base,(label,'metadata/named data')
    assert nontext(obj)==data,(label,'nontext')
    assert len(b.sizes(str(obj)))==16 and not cell.get('losses')
    carrier='_ZN22V90ConnectionEvaluator26evaluateMeanErrorStdPhase3Ef'
    extra={carrier} if label=='retrain-shared-cleanup-post-counter-verdict' else set()
    assert set(cell.get('changed_bodies',[]))<=targets|extra,(label,cell.get('changed_bodies'))
    for stage in ('01.rtl','20.combine','21.ce2','25.greg','31.bbro','33.sched2'):
        dump=(folder/label/('V90ConnectionEvaluator.cpp.'+stage)).read_text()
        bodies=re.split(r'^;; Function ',dump,flags=re.M)[1:]
        for name in ('updateAvePdsnr','indicateLocalRetrain','indicateRemoteRetrain'):
            selected=[s for s in bodies if 'V90ConnectionEvaluator::'+name+'(' in s.splitlines()[0]]
            assert len(selected)==1,(label,stage,name)
            body=selected[0];records[label+'/'+stage+'/'+name]=instructions(body)
            if stage=='01.rtl' and name=='updateAvePdsnr':
                member='member-product' in label or label=='combined-three-gains'
                assert body.count('<variable>.avePdsnrNofSymbols+0')==(4 if member else 3),(label,'explicit member reference')
    reports[label]={'gains':cell.get('gains',[]),'changed_bodies':cell.get('changed_bodies',[])}
winner=folder/'combined-three-gains/candidate.o'
for target in targets:assert b.verdict(*b.body(b.BLOB,target),*b.body(str(winner),target))==('EXACT',0)
assert len(cells)==9 and len(records)==162
(root/'complete-tu-audit.json').write_text(json.dumps({'valid_cells':len(cells),'functions_per_cell':16,'reports':reports,'stage_records':len(records),'initial_count_reference_controls':9},indent=2)+'\n')
(root/'stage-patterns.json').write_text(json.dumps(records,indent=2)+'\n')
print('9/9 complete TUs, 16 bodies each; adopted thirteen bystanders/metadata/data/nontext relocations unchanged, no exact losses; one negative-control bystander carrier retained')
print('162/162 RTL records; 9/9 initial member-count-reference controls; combined local136B/remote136B/average113B EXACT; TU8→11/16')
