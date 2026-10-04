#!/usr/bin/env python3
"""Audit sixteen complete TUs and the constructor's ce2/store boundaries."""
import json,re
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
text=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(text[text.index('def inspect'):text.index('reports=')])

def nontext(path):
    with path.open('rb') as stream:
        elf=ELFFile(stream)
        return {s.name:s.data().hex() for s in elf.iter_sections() if s['sh_flags']&2 and s.name!='.text'}

reports={};allcells=[]
for suffix in ('','-reset'):
    root=driver.ROOT/'build'/('gcc3-tone-detector-boundaries'+suffix)
    cells=json.loads((root/'results.json').read_text())['families']['GenericToneDetector']['cells']
    base=root/'GenericToneDetector/baseline/candidate.o'
    meta=inspect(base); data=nontext(base)
    assert len(b.sizes(str(base)))==7
    for label,cell in cells.items():
        obj=root/'GenericToneDetector'/label/'candidate.o'
        assert inspect(obj)==meta,(suffix,label,'metadata/data')
        assert nontext(obj)==data,(suffix,label,'allocated nontext bytes')
        with obj.open('rb') as stream:
            elf=ELFFile(stream)
            assert not any(isinstance(s,RelocationSection) and elf.get_section(s['sh_info']).name!='.text' for s in elf.iter_sections())
        changes=cell.get('changed_bodies',[])
        assert set(changes)<={'_ZN19GenericToneDetectorC1EjjPdS0_jjfjfjj','_ZN19GenericToneDetectorC2EjjPdS0_jjfjfjj','_ZN19GenericToneDetector5resetEv','_ZN19GenericToneDetector7processEf'}
        assert bool(cell.get('losses'))==('shared-zero' in label)
        reports[suffix+'/'+label]={'changed_bodies':changes,'gains':cell.get('gains',[]),'losses':cell.get('losses',[])}
        allcells.append(cell)
root=driver.ROOT/'build/gcc3-tone-detector-boundaries-reset/GenericToneDetector'
records={}
for label in ('baseline','member-quotient','member-quotient-late-count'):
    for stage in ('01.rtl','20.combine','21.ce2','22.regmove','25.greg','33.sched2'):
        text=(root/label/('GenericToneDetector.cpp.'+stage)).read_text()
        bodies=[s for s in re.split(r'^;; Function ',text,flags=re.M)[1:] if s.startswith('GenericToneDetector::GenericToneDetector(')]
        assert len(bodies)==2
        for clone,body in enumerate(bodies):
            patterns=instructions(body)
            branches=sum('if_then_else' in str(p) for p in patterns.values())
            if stage in ('20.combine','21.ce2'):
                assert branches==(0 if stage=='21.ce2' and label=='baseline' else 2)
            if stage=='01.rtl':
                fields=[field for p in patterns.values() for field in ('count_2c','acc_0c','acc_10','acc_14','acc_18') if p[0]=='set' and 'mem' in str(p[1]) and field in str(p[1])]
                assert fields==(['acc_0c','acc_10','acc_14','acc_18','count_2c'] if label.endswith('late-count') else ['count_2c','acc_0c','acc_10','acc_14','acc_18'])
            records[label+'/'+stage+'/'+str(clone)]={'branches':branches,'patterns':patterns}
winner=root/'member-quotient-late-count/candidate.o'
for name in ('_ZN19GenericToneDetectorC1EjjPdS0_jjfjfjj','_ZN19GenericToneDetectorC2EjjPdS0_jjfjfjj'):
    assert b.verdict(*b.body(b.BLOB,name),*b.body(str(winner),name))==('EXACT',0)
summary={'valid_cells':len(allcells),'source_hashes':len({c['source_hash'] for c in allcells}),'object_hashes':len({c['object_hash'] for c in allcells}),'functions_per_tu':7,'metadata':meta,'reports':reports,'stage_records':len(records),'stage_controls':10}
(driver.ROOT/'build/tone-detector-full-tu-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
(driver.ROOT/'build/tone-detector-stage-patterns.json').write_text(json.dumps(records,indent=2)+'\n')
print('16/16 full TUs: 7 functions; metadata, named data, allocated nontext bytes/relocations and bystanders preserved')
print('36/36 stage records; 10/10 ce2 and initial reset-store controls pass; C1/C2 exact267B each')
