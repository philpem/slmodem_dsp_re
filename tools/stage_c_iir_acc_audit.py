#!/usr/bin/env python3
"""Audit the bounded IIR short accumulator control and its return RTL."""
import argparse,hashlib,json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_reload_trace import function,instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text();exec(s[s.index('def nontext'):s.index('\nreports=')])
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run-dir',type=Path,default=d.ROOT/'build/stage-c-iir-acc');root=p.parse_args().run_dir
r=json.loads((root/'results.json').read_text());f=r['families']['fpm_iir'];base=root/'fpm_iir/baseline/candidate.o';rows=[];records={}
stages=('01.rtl','20.combine','24.lreg','25.greg','28.peephole2','30.rnreg','33.sched2')
for label,c in f['cells'].items():
 obj=root/'fpm_iir'/label/'candidate.o'
 assert hashlib.sha256(obj.read_bytes()).hexdigest()==c['object_hash']
 assert inspect(obj)==inspect(base) and nontext(obj)==nontext(base)
 ns=sorted(b.sizes(str(obj)));assert ns==sorted(c['functions'])
 changes=[n for n in ns if b.body(str(base),n)!=b.body(str(obj),n)];assert changes==c.get('changed_bodies',[])
 assert set(changes)<= {'FPM_iir_filt','FPM_iir_filt_block'}
 actual={n:list(b.verdict(*b.body(b.BLOB,n),*b.body(str(obj),n))) for n in c['verdicts']};assert actual==c['verdicts']
 assert not c.get('losses') and not c.get('gains')
 if label=='baseline':assert obj.read_bytes()==(root/'fpm_iir/retained.o').read_bytes()
 records[label]={}
 for st in stages:
  path=root/'fpm_iir'/label/('fpm_iir.c.'+st);stream=instructions(function(path.read_text(),'FPM_iir_filt'))
  assert 'sign_extend:SI' in str(stream[110])
  if label=='baseline':assert 'sign_extend:SI' in str(stream[123])
  elif st=='01.rtl':assert stream[123][2][0].startswith('reg/v:SI')
  else:assert 123 not in stream and stream[131][2][0].startswith('reg/v:SI')
  records[label][st]={'instruction_count':len(stream),'dump_hash':hashlib.sha256(path.read_bytes()).hexdigest(),'selected':{u:stream[u] for u in (110,123,124,131) if u in stream}}
 rows.append({'cell':label,'changed':changes,'verdicts':actual,'metadata_nontext_identical':True})
assert len(rows)==2
result={'complete_TU_cells':2,'shared_body_verdicts':6,'raw_controls':1,'stage_streams':14,'exact_gains':0,'exact_losses':0,'rows':rows,'return_boundary_first_change':'01.rtl UID123 sign_extendSI->copy;combine removescopy and connects131 directly','records':records,'limits':'Return operation recovered; original separate output/feedback register carriers and other arithmetic/owner allocation remain non-exact.'}
(root/'audit.json').write_text(json.dumps(result,indent=2)+'\n');print('2 completeTU cells;6 body verdicts;1 rawbaseline;14 returnstage streams;II/data/bindings unchanged;0exactgains/losses')
