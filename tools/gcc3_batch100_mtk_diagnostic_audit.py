#!/usr/bin/env python3
"""Audit complete original-header, library and conditional/precision diagnostics."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b;s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
specs={'mtk-profile':4,'mtk-inline-header':4,'mtk-library':7,'mtk-conditional':5,'mtk-carrier':5};reports={};total=0
for key,count in specs.items():
 root=d.ROOT/('build/gcc3-batch100-'+key);r=json.loads((root/'results.json').read_text());cells=r['families']['PHASOR']['cells'];assert len(cells)==count and cells['baseline']['baseline_reproduced']
 for label,c in cells.items():
  p=root/'PHASOR'/label/'candidate.o';q=inspect(p)
  with p.open('rb') as f:
   e=ELFFile(f);nontext={x.name:x.data().hex() for x in e.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
   relocs=[(x.name,y['r_offset'],y['r_info_type'],y['r_info_sym']) for x in e.iter_sections() if isinstance(x,RelocationSection) and e.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
  assert q['objects']=={} and not relocs
  if label=='baseline':base=q;bn=nontext
  inline=label.startswith(('fastmath-','inline-'));expected_records=dict(base['records']);expected_sizes=dict(base['allocated_sizes']);expected_nontext=dict(bn)
  if inline:
   expected_records.pop('fmodf')
   if key=='mtk-library' and label.endswith('double-double-mod'):
    pass
   elif key=='mtk-carrier' and label.startswith('inline-phase-1-'):
    expected_sizes['.rodata.cst8']=32;expected_nontext['.rodata.cst8']=bn['.rodata.cst8']+'0000000000000000';expected_nontext['.rodata.cst4']='db0fc9400000803f'
   else:
    expected_sizes['.rodata.cst4']=12;expected_nontext['.rodata.cst4']='db0fc940'+bn['.rodata.cst4']
  assert q['records']==expected_records and q['allocated_sizes']==expected_sizes and nontext==expected_nontext,(key,label,'unexpected profile effect')
  assert not c.get('losses') and set(c.get('changed_bodies',[]))<={'MTK_phasor'}
  rows=b.insns(p,'MTK_phasor');assert sum(x[0]=='fprem' for x in rows)==int(inline)
  reports[key+'/'+label]={'metadata':q,'nontext':nontext,'nontext_relocs':relocs,'verdict':c['verdicts']['MTK_phasor'],'FPREM':sum(x[0]=='fprem' for x in rows),'calls':[x for x in rows if x[0]=='call'],'changed_bodies':c.get('changed_bodies',[])};total+=1
(d.ROOT/'build/batch100-mtk-diagnostic-complete-audit.json').write_text(json.dumps({'valid_tus':total,'reports':reports},indent=2)+'\n')
print('%d/%d full MTK diagnostic TU audits; explicit import/constant mode deltas accounted; no exact gain/loss'%(total,total))
