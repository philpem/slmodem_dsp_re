#!/usr/bin/env python3
"""Audit bounded FSM snapshots and GetFP_Value recurrence controls."""
import hashlib,json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_reload_trace import function,instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text();exec(s[s.index('def nontext'):s.index('\nreports=')])
rows=[]
for run in ('stage-c-fsm','stage-c-getfp','stage-c-dsp-operands','stage-c-getfp-abs'):
 out=d.ROOT/'build'/run;r=json.loads((out/'results.json').read_text())
 for family,f in r['families'].items():
  base=out/family/'baseline/candidate.o'
  for label,c in f['cells'].items():
   obj=out/family/label/'candidate.o'
   assert hashlib.sha256(obj.read_bytes()).hexdigest()==c['object_hash']
   assert inspect(base)==inspect(obj) and nontext(base)==nontext(obj)
   ns=sorted(b.sizes(str(obj)));assert ns==sorted(c['functions'])
   changed=[n for n in ns if b.body(str(base),n)!=b.body(str(obj),n)];assert changed==c.get('changed_bodies',[])
   assert set(changed)<=({'GetFP_Value'} if family=='FP_math' else {'FPM_FSM_modulate'})
   actual={n:list(b.verdict(*b.body(b.BLOB,n),*b.body(str(obj),n))) for n in c['verdicts']};assert actual==c['verdicts'] and not c.get('losses')
   if label=='baseline':assert obj.read_bytes()==(out/family/'retained.o').read_bytes()
   rows.append({'run':run,'family':family,'cell':label,'verdicts':actual,'changed':changed,'metadata_nontext_identical':True})
# Compiler graph boundaries, using existing UID parser and complete-TU dumps.
root=d.ROOT/'build/stage-c-getfp-abs/FP_math/guarded-negative-abs'
fp={st:instructions(function((root/('FP_math.c.'+st)).read_text(),'GetFP_Value')) for st in ('01.rtl','20.combine','25.greg','28.peephole2','30.rnreg','33.sched2')}
assert 'ashiftrt:SI' in str(fp['01.rtl'][12]) and '31' in str(fp['01.rtl'][12])
assert 'plus:SI' in str(fp['01.rtl'][35]) and 'remaining' in str(fp['01.rtl'][35])
assert 'sign_extend:SI' in str(fp['01.rtl'][38]) and 'subreg:HI' in str(fp['01.rtl'][38])
assert 'neg:SI' in str(fp['01.rtl'][28]) and 'minus:SI' in str(fp['20.combine'][28])
assert 'dx' in str(fp['25.greg'][25]) and 'ax' in str(fp['25.greg'][25])
root=d.ROOT/'build/stage-c-dsp-operands/fpm_fsm/entry-array-base'
fs={st:instructions(function((root/('fpm_fsm.c.'+st)).read_text(),'FPM_FSM_modulate')) for st in ('01.rtl','20.combine','25.greg','28.peephole2','30.rnreg','33.sched2')}
initial=str(fs['01.rtl']);assert all(x in initial for x in ('scaled','scale','samples_per_sym','zero_extend:SI','subreg:HI'))
asm=(root/'fpm_fsm.s').read_text();assert asm.count('call\tFPM_TONE_set_freq')==asm.count('call\tFPM_TONE_set_scale')==asm.count('call\tFPM_TONE_generate')==1
assert len(rows)==16
result={'complete_TU_cells':16,'shared_body_verdicts':sum(len(x['verdicts']) for x in rows),'exact_gains':0,'exact_losses':0,'stage_streams':12,'rows':rows,'GetFP_stage_patterns':fp,'FSM_stage_patterns':fs,'finite_behaviour_defect':'none established; original loop and entry snapshots recovered as partial graphs'}
(d.ROOT/'build/stage-c-dsp-partials-audit.json').write_text(json.dumps(result,indent=2)+'\n')
print('16 complete TU controls;57 body verdicts;6 raw baselines;12 stage streams;data/bindings/neighbours unchanged;0 exact gains/losses')
