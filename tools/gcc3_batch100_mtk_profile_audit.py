#!/usr/bin/env python3
"""Full MTK source/profile audit; explicit measured import/constant deltas."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b;s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
root=d.ROOT/'build/gcc3-batch100-mtk-profile';r=json.loads((root/'results.json').read_text());cells=r['families']['PHASOR']['cells'];assert len(cells)==4 and cells['baseline']['baseline_reproduced'];reports={}
for label,c in cells.items():
 p=root/'PHASOR'/label/'candidate.o';q=inspect(p)
 with p.open('rb') as f:
  e=ELFFile(f);nontext={x.name:x.data().hex() for x in e.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
  relocs=[(x.name,y['r_offset'],y['r_info_type'],y['r_info_sym']) for x in e.iter_sections() if isinstance(x,RelocationSection) and e.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
 assert q['objects']=={} and not relocs
 if label=='baseline':base=q;bn=nontext
 expected_records=dict(base['records']);expected_sizes=dict(base['allocated_sizes']);expected_nontext=dict(bn)
 if label.startswith('fastmath-'):
  expected_records.pop('fmodf');expected_sizes['.rodata.cst4']=12;expected_nontext['.rodata.cst4']='db0fc940'+bn['.rodata.cst4']
 assert q['records']==expected_records and q['allocated_sizes']==expected_sizes and nontext==expected_nontext,(label,'unexpected profile effect')
 assert not c.get('losses') and set(c.get('changed_bodies',[]))<={'MTK_phasor'}
 rows=b.insns(p,'MTK_phasor');assert sum(x[0]=='fprem' for x in rows)==int(label.startswith('fastmath-'))
 reports[label]={'metadata':q,'nontext':nontext,'nontext_relocs':relocs,'verdict':c['verdicts']['MTK_phasor'],'FPREM':sum(x[0]=='fprem' for x in rows),'calls':[x for x in rows if x[0]=='call'],'changed_bodies':c.get('changed_bodies',[])}
(root/'complete-object-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('4/4 MTK full-TU source/profile audits pass; only fast-math removes fmodf import and adds float6Pi constant; no named data/bystanders; no exact gain/loss')
