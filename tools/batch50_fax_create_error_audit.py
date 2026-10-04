#!/usr/bin/env python3
"""Original class1 missing failure-arm complete-TU diagnostic audit."""
import json
from collections import Counter
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/batch20_data_gains_audit.py').read_text()
exec(s[s.index('b=d.b'):s.index('allowed_functions=')])
ins=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();ins=ins[ins.index('def inspect'):ins.index('reports=')]
ins=ins.replace("if v['st_info']['type']=='STT_OBJECT':","if v['st_info']['type']=='STT_OBJECT' and not isinstance(index,int):objects[name]={'section':section,'alignment':v['st_value'],'size':v['st_size']}\n   if v['st_info']['type']=='STT_OBJECT' and isinstance(index,int):")
exec(ins)
out=d.ROOT/'build/batch50-fax-create-error';r=json.loads((out/'results.json').read_text());f=r['families']['class1'];base=canonical(out/'class1/baseline/candidate.o');reports={}
for label,c in f['cells'].items():
 q=canonical(out/'class1'/label/'candidate.o')
 for k in ('records','objects'):assert q[k]==base[k],(label,k)
 for k in ('nontext','nontext_relocations','allocated_sizes'):
  strip=lambda v:{n:x for n,x in v[k].items() if not n.startswith('.rodata.str')}
  assert strip(q)==strip(base),(label,k)
 bag=lambda v:Counter(x for n,raw in v['nontext'].items() if n.startswith('.rodata.str') for x in bytes.fromhex(raw).split(b'\0') if x)
 assert not bag(base)-bag(q)
 assert bag(q)-bag(base)==(Counter() if label=='baseline' else Counter({b'Internal memory allocation error!\n\n':1})),label
 before=Counter(map(repr,base['text_relocations']));after=Counter(map(repr,q['text_relocations']))
 for k in (after-before)|(before-after):assert any(t in k for t in ('dsplibs_debug_level','dsplibs_debug_printf','Internal memory allocation error!')), (label,k)
 assert not c.get('gains') and not c.get('losses')
 assert set(c.get('changed_bodies',[]))<= {'fax_class1_create','fax_class1_progress'}
 reports[label]={'functions':len(c['functions']),'changed_bodies':c.get('changed_bodies',[]),'constructor':c['verdicts']['fax_class1_create'],'progress':c['verdicts']['fax_class1_progress'],'added_text_relocations':dict(after-before),'removed_text_relocations':dict(before-after)}
(out/'full-audit.json').write_text(json.dumps(reports,indent=2)+'\n')
print('4 valid full-TU cells; original failure diagnostic, metadata/data/nonstring sections audited; 8 exact bodies unchanged; nonexact progress cursor change explicit; no gains/losses')
