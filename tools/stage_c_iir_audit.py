#!/usr/bin/env python3
"""Audit full IIR controls and trace the untouched form-I scratch collateral."""
import argparse,hashlib,json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_reload_trace import function,instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text();exec(s[s.index('def nontext'):s.index('\nreports=')])
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir',type=Path,default=d.ROOT/'build')
build=parser.parse_args().build_dir
rows=[]
for run in ('stage-c-iir','stage-c-iir-narrow'):
 root=build/run;ledger=json.loads((root/'results.json').read_text());family=ledger['families']['fpm_iir'];base=root/'fpm_iir/baseline/candidate.o'
 for label,c in family['cells'].items():
  obj=root/'fpm_iir'/label/'candidate.o';assert hashlib.sha256(obj.read_bytes()).hexdigest()==c['object_hash']
  assert inspect(obj)==inspect(base) and nontext(obj)==nontext(base)
  ns=sorted(b.sizes(str(obj)));assert ns==sorted(c['functions'])
  changes=[n for n in ns if b.body(str(base),n)!=b.body(str(obj),n)];assert changes==c.get('changed_bodies',[])
  actual={n:list(b.verdict(*b.body(b.BLOB,n),*b.body(str(obj),n))) for n in c['verdicts']};assert actual==c['verdicts']
  assert not c.get('losses') and not c.get('gains')
  if label=='baseline':assert obj.read_bytes()==(root/'fpm_iir/retained.o').read_bytes()
  rows.append({'run':run,'cell':label,'changed':changes,'verdicts':actual,'metadata_nontext_identical':True})
# Complete untouched II streams agree through flow2; first change is peephole2.
root=build/'stage-c-iir-narrow/fpm_iir';trace={}
for stage in ('01.rtl','04.jump','06.cse','14.ce1','19.life','20.combine','21.ce2','22.regmove','24.lreg','25.greg','27.flow2','28.peephole2','30.rnreg','33.sched2'):
 streams={cell:instructions(function((root/cell/('fpm_iir.c.'+stage)).read_text(),'FPM_iir_filt_II')) for cell in ('baseline','ff-before-clamp')}
 a=streams['baseline'];c=streams['ff-before-clamp'];assert set(a)==set(c)
 diffs=[u for u in a if a[u]!=c[u]]
 if stage not in ('28.peephole2','30.rnreg','33.sched2'):assert not diffs
 if stage=='28.peephole2':assert set(diffs)=={210,211}
 if stage in ('30.rnreg','33.sched2'):assert set(diffs)=={30,185,210,211}
 trace[stage]={'instructions_per_cell':len(a),'changed_UIDs':diffs,'patterns':{cell:{u:v for u,v in stream.items() if u in (30,185,210,211)} for cell,stream in streams.items()}}
# Feedforward conversion is born before the clamp in initial RTL, not reload.
scalar={}
for stage in ('01.rtl','20.combine','25.greg','28.peephole2','30.rnreg','33.sched2'):
 stream=instructions(function((root/'ff-before-clamp'/('fpm_iir.c.'+stage)).read_text(),'FPM_iir_filt'))
 assert 'sign_extend:SI' in str(stream[74]) and ('subreg:HI' in str(stream[74]) or 'reg:HI' in str(stream[74]))
 scalar[stage]={'instructions':len(stream),'feedforward_UID74':stream[74]}
assert len(rows)==7
report={'cells':7,'body_verdicts':21,'raw_controls':2,'exact_gains':0,'exact_losses':0,'rows':rows,'untouched_II_first_changed_pass':'28.peephole2','untouched_II_trace':trace,'scalar_feedforward_trace':scalar,'limits':'No original RTL exists; stage boundaries concern controlled builds. Do not infer the original scratch cursor from emitted operands.'}
(build/'stage-c-iir-audit.json').write_text(json.dumps(report,indent=2)+'\n')
print('7 full-TU controls,21 body verdicts,2 raw baselines;20 stage streams;metadata/data/bindings fixed;II first changes at peephole2, ff conversion born initialRTL74')
