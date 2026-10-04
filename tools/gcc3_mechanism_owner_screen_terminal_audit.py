#!/usr/bin/env python3
"""Complete-TU terminal controls plus asserting earliest sibling-pass witness."""
import json,re
import playbook_small_patterns as d
b=d.b
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text();exec(s[s.index('def nontext'):s.index('\nreports=')])
reports=[];stages=[]
for domain in ('terminal','adid-terminal','v90-reset-result'):
 out=d.ROOT/'build'/('gcc3-mechanism-owner-screen-'+domain)
 r=json.loads((out/'results.json').read_text())
 for family,f in r['families'].items():
  base=out/family/'baseline/candidate.o';meta=inspect(base);data=nontext(base)
  for label,c in f['cells'].items():
   obj=out/family/label/'candidate.o'
   assert inspect(obj)==meta,(family,label,'complete metadata/named data')
   assert nontext(obj)==data,(family,label,'allocated nontext/canonical relocations')
   assert not c.get('losses'),(family,label,'exact neighbour loss')
   expected=[] if label=='baseline' or domain=='adid-terminal' else [('_ZN8V92Modem5resetEv' if family=='V92Modem' else '_ZN8V90Modem5resetEj')]
   assert c.get('changed_bodies',[])==expected,(family,label,'all bystanders unchanged')
   reports.append({'family':family,'label':label,'functions':len(c['verdicts']),'changed':c.get('changed_bodies',[]),'gains':c.get('gains',[]),'losses':c.get('losses',[]),'nontext':'identical','metadata':'identical','raw_baseline':c.get('baseline_reproduced',False)})
  if family in ('V90Modem','V92Modem'):
   for label in f['cells']:
    for stage in ('01.rtl','02.sibling','35.mach'):
     text=(out/family/label/(family+'.cpp.'+stage)).read_text()
     chunks=[x for x in re.split(r'^;; Function ',text,flags=re.M)[1:] if x.startswith('void '+family+'::reset(')]
     assert len(chunks)==1
     calls=re.findall(r'\(call_insn/j[\s\S]*?(?=\n\n)',chunks[0])
     diagnostic=sum('"dsplibs_debug_printf"' in x for x in calls)
     expected=int(label in ('terminal-default-return','terminal-probe-result') and stage!='01.rtl')
     assert diagnostic==expected,(family,label,stage,diagnostic,expected)
     stages.append({'family':family,'label':label,'stage':stage,'terminal_diagnostic_sibling':diagnostic,'all_siblings':len(calls)})
assert len(reports)==10 and len(stages)==24
out=d.ROOT/'build/gcc3-mechanism-owner-screen-terminal-audit.json'
out.write_text(json.dumps({'cells':10,'compared_body_verdicts':130,'reports':reports,'stages':stages,'first_terminal_diagnostic_selection':'02.sibling'},indent=2)+'\n')
print('10 complete-TU cells; 130 strict body verdicts; all metadata/nontext unchanged; 24 stage controls: terminal diagnostic selected at02.sibling')
