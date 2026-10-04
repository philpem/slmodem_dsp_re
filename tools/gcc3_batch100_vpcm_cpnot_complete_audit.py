#!/usr/bin/env python3
"""VPcm CPnot data ledger; string order changes retained as controlled nontext."""
import json,collections
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text();exec(s[s.index('def nontext'):s.index('\nreports=')])
def strings(path):
 with path.open('rb') as f:
  elf=ELFFile(f);return collections.Counter(x for sec in elf.iter_sections() if sec.name.startswith('.rodata.str') for x in sec.data().split(b'\0') if x)
reports={}
for domain in ('cpnot-length-owner',):
 out=d.ROOT/'build'/('gcc3-batch100-vpcm-'+domain);r=json.loads((out/'results.json').read_text())
 for family,f in r['families'].items():
  base=out/family/'baseline/candidate.o';meta=inspect(base);data=nontext(base)
  for label,c in f['cells'].items():
   obj=out/family/label/'candidate.o';m=inspect(obj);n=nontext(obj)
   assert strings(obj)==strings(base),(domain,label,'string multiplicity')
   assert m['records']==meta['records'] and m['objects']==meta['objects'],(domain,label,'metadata/named data')
   diffs={k:{'before':data.get(k),'after':n.get(k)} for k in set(data)|set(n) if data.get(k)!=n.get(k)}
   assert set(diffs)<= {'.rodata','.rodata.str1.1','.rodata.str1.4'},(domain,label,set(diffs))
   reports[domain+'/'+label]={'named_data_before':meta,'named_data_after':m,'nontext_changes':diffs,'functions':len(c['verdicts']),'verdicts':c['verdicts'],'gains':c.get('gains',[]),'losses':c.get('losses',[]),'changed':c.get('changed_bodies',[]),'status':'negative source control, no production adoption'}
(d.ROOT/'build/batch100-vpcm-cpnot-complete-audit.json').write_text(json.dumps({'cells':len(reports),'reports':reports},indent=2,default=lambda x: {'raw_bytes':x.hex()} if isinstance(x,bytes) else None)+'\n')
print(len(reports),'full TU changed-data ledgers; metadata/named data assertions and string multiplicity controls')
