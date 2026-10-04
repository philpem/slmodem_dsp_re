#!/usr/bin/env python3
"""Audit runtime-discriminated callback order and validation reread controls."""
import json,re
from collections import Counter
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=driver.b;s=(driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
root=driver.ROOT/'build/gcc3-batch20-callprog-callback';cells=json.loads((root/'results.json').read_text())['families']['Callprog']['cells'];base=inspect(root/'Callprog/baseline/candidate.o');reports={}
def nodes(x):
 if isinstance(x,list):
  yield x
  for y in x[1:]:yield from nodes(y)
for label,c in cells.items():
 p=root/'Callprog'/label/'candidate.o';q=inspect(p)
 assert q==base,(label,'metadata/data')
 with p.open('rb') as stream:
  elf=ELFFile(stream);symtab=elf.get_section_by_name('.symtab')
  nontext={x.name:(Counter(x.data().split(b'\0')) if x.name=='.rodata.str1.1' else x.data().hex()) for x in elf.iter_sections() if x['sh_flags']&2 and x.name!='.text'}
  extra=[(x.name,y['r_offset'],y['r_info_type'],symtab.get_symbol(y['r_info_sym']).name,elf.get_section(x['sh_info']).data()[y['r_offset']:y['r_offset']+4].hex()) for x in elf.iter_sections() if isinstance(x,RelocationSection) and elf.get_section(x['sh_info']).name!='.text' for y in x.iter_relocations()]
 if label=='baseline':base_nontext,base_extra=nontext,extra
 assert nontext==base_nontext and extra==base_extra
 assert not c.get('gains') and not c.get('losses')
 assert set(c.get('changed_bodies',[]))<={'CALLPROG_Dial'}
 text=(root/'Callprog'/label/'Callprog.c.01.rtl').read_text();parts=[x for x in re.split(r'^;; Function ',text,flags=re.M)[1:] if x.splitlines()[0].strip()=='CALLPROG_Dial'];assert len(parts)==1
 pats=instructions(';; Function '+parts[0]);getters=[uid for uid,x in pats.items() if any(n[0].startswith('symbol_ref') and any('modem_get_param' in str(v) for v in n[1:]) for n in nodes(x))]
 expected=9 if label=='baseline' else 11+(1 if 'validation-reread-1' in label else 0)
 assert len(getters)==expected,(label,len(getters),expected)
 reports[label]={'verdict':c['verdicts']['CALLPROG_Dial'],'getters':getters,'functions':len(c['functions']),'data':len(base['objects'])}
assert len(cells)==5
final=root/'Callprog/diagnostic-first-1-validation-reread-1/candidate.o'
production=driver.ROOT/'build/period/src_callprog_Callprog.o'
assert production.is_file(), 'run make period T=t_callprog_create first'
assert final.read_bytes()==production.read_bytes(), 'final production raw mismatch'
assert cells['baseline']['baseline_reproduced']
(root/'complete-audit.json').write_text(json.dumps({'valid_tus':5,'getter_controls':5,'raw_baseline_reproduced':True,'raw_final_production_reproduced':True,'reports':reports},indent=2)+'\n')
print('5/5 complete TUs preserve metadata/data/nontext/relocations and bystanders; 5/5 initial RTL getter controls fire (9/11/12/11/12); no exact gain/loss')
