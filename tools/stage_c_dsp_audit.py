#!/usr/bin/env python3
"""Audit complete DSP candidate TUs and retain initial/final stage witnesses."""
import argparse, hashlib, json, re
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text();exec(s[s.index('def nontext'):s.index('\nreports=')])
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--run-dir', type=Path, default=d.ROOT/'build/stage-c-dsp')
out=parser.parse_args().run_dir
r=json.loads((out/'results.json').read_text());rows=[];stages=[]
for family,f in r['families'].items():
 base=out/family/'baseline/candidate.o';metadata=inspect(base);data=nontext(base)
 for label,c in f['cells'].items():
  obj=out/family/label/'candidate.o'
  assert hashlib.sha256(obj.read_bytes()).hexdigest()==c['object_hash'],(family,label,'object hash')
  names=sorted(b.sizes(str(obj)))
  assert names==sorted(c['functions']),(family,label,'function inventory')
  changed=[name for name in names if b.body(str(base),name)!=b.body(str(obj),name)]
  assert changed==c.get('changed_bodies',[]),(family,label,'body changes')
  actual={name:list(b.verdict(*b.body(b.BLOB,name),*b.body(str(obj),name))) for name in c['verdicts']}
  assert actual==c['verdicts'],(family,label,'body verdicts')
  assert inspect(obj)==metadata,(family,label,'metadata')
  assert nontext(obj)==data,(family,label,'nontext')
  assert not c.get('losses'),(family,label,'exact losses')
  assert c.get('changed_bodies',[])==([] if label=='baseline' else ['notch' if family=='Notch' else 'FPM_MTD_detect'])
  row={'family':family,'cell':label,'verdicts':c['verdicts'],'metadata_identical':True,'nontext_identical':True,'raw_control':c.get('baseline_reproduced',False)}
  rows.append(row)
  for suffix in ('01.rtl','20.combine','25.greg','28.peephole2','33.sched2','34.stack','35.mach'):
   text=(out/family/label/(family+'.c.'+suffix)).read_text()
   name='notch' if family=='Notch' else 'FPM_MTD_detect'
   chunks=[x for x in re.split(r'^;; Function ',text,flags=re.M)[1:] if x.split()[0]==name]
   assert len(chunks)==1,(family,label,suffix)
   text=chunks[0]
   stages.append({'family':family,'cell':label,'stage':suffix,'HI_register_mentions':len(re.findall(r'reg[^\n]*:HI',text)),'HI_memory_mentions':text.count('mem:HI'),'plus_SF':text.count('plus:SF'),'plus_HI':text.count('plus:HI'),'plus_SI':text.count('plus:SI')})
assert len(rows)==6 and len(stages)==42
report={'cells':6,'body_verdicts':sum(len(x['verdicts']) for x in rows),'stage_windows':42,'rows':rows,'stage_counters':stages}
(out/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
print('6/6 complete TU controls: metadata, named data, allocated nontext and relocations identical; 14 body verdicts; 42 retained stage windows')
