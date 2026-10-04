#!/usr/bin/env python3
"""Audit closed timing/constructor/precoder controls as complete translation units."""
import json
from pathlib import Path
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b
text=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(text[text.index('def inspect'):text.index('reports=')])
text=(driver.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text()
exec(text[text.index('def nontext'):text.index('\nreports=')])
reports={};total=0
for suffix,family,count in (('timing','ResamplerTiming',14),('resampler-ctor','Resampler',25),('precoder','V92Precoder',8),('resampler-init','Resampler',25),('precoder-cfg','V92Precoder',8)):
    root=driver.ROOT/'build'/('gcc3-batch20-v90-'+suffix)
    cells=json.loads((root/'results.json').read_text())['families'][family]['cells']
    base=root/family/'baseline/candidate.o';meta=inspect(base);data=nontext(base)
    for label,cell in cells.items():
        obj=root/family/label/'candidate.o';assert len(b.sizes(str(obj)))==count,(family,label)
        assert inspect(obj)==meta,(family,label,'metadata/named data')
        assert nontext(obj)==data,(family,label,'nontext')
        assert not cell.get('losses'),(family,label,'losses')
        reports[family+'/'+label]={'gains':cell.get('gains',[]),'changed_bodies':cell.get('changed_bodies',[]),'functions':count}
        total+=1
assert total==35,total
(driver.ROOT/'build/batch20-resampler-negative-full-tu-audit.json').write_text(json.dumps({'valid_cells':total,'reports':reports},indent=2)+'\n')
print('35/35 full-TU metadata/named-data/nontext relocations/anonymous constants controls, no exact losses')
