#!/usr/bin/env python3
"""Trace the measured MTD conversion and result-carrier pass boundaries."""
import hashlib,json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_reload_trace import function,instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text();exec(s[s.index('def nontext'):s.index('\nreports=')])
stages=('01.rtl','04.jump','06.cse','14.ce1','19.life','20.combine','21.ce2','22.regmove','24.lreg','25.greg','26.postreload','27.flow2','28.peephole2','30.rnreg','33.sched2')
roots={'short_mask':d.ROOT/'build/stage-c-mtd-carrier/fpm_mtd/baseline','int_mask':d.ROOT/'build/stage-c-mtd-carrier/fpm_mtd/post-mask-int-carrier'}
records={};streams={}
for label,root in roots.items():
 records[label]={};streams[label]={}
 for stage in stages:
  path=root/('fpm_mtd.c.'+stage);text=function(path.read_text(),'FPM_MTD_detect');stream=instructions(text);streams[label][stage]=stream
  selected={uid:pat for uid,pat in stream.items() if (97<=uid<=149 or 209<=uid<=215 or 230<=uid<=232)}
  records[label][stage]={'dump_hash':hashlib.sha256(path.read_bytes()).hexdigest(),'instruction_denominator':len(stream),'selected_patterns':selected,'selected_order':list(selected)}
short=streams['short_mask'];wide=streams['int_mask']
assert 'sign_extend:SI' in str(short['01.rtl'][106])
assert all('sign_extend:SI' in str(short[stage][106]) for stage in stages)
assert 'lt:QI' in str(short['01.rtl'][140]) and 'zero_extend:SI' in str(short['01.rtl'][141])
assert 141 not in short['20.combine'] and 'zero_extend:SI' in str(short['20.combine'][142])
assert list(short['20.combine']).index(117)>list(short['20.combine']).index(111)
assert list(short['21.ce2']).index(117)<list(short['21.ce2']).index(111)
assert 230 not in short['27.flow2'] and all(uid in short['28.peephole2'] for uid in (230,231,232))
assert 'strict_low_part' in str(short['28.peephole2'][232])
assert 'cx' in str(short['25.greg'][149]) and 'ax' in str(short['25.greg'][149])
assert 'sign_extend:SI' in str(wide['01.rtl'][125])
assert 125 in wide['19.life'] and 125 not in wide['20.combine']
assert '117' in str(wide['20.combine'][126]) and 'sign_extend' not in str(wide['20.combine'][126])
# The readout is checked against actual complete objects, not ledger claims alone.
run=d.ROOT/'build/stage-c-mtd-carrier';r=json.loads((run/'results.json').read_text());f=r['families']['fpm_mtd'];base=roots['short_mask']/'candidate.o';audits=[]
for label,c in f['cells'].items():
 obj=run/'fpm_mtd'/label/'candidate.o'
 assert hashlib.sha256(obj.read_bytes()).hexdigest()==c['object_hash']
 assert inspect(obj)==inspect(base) and nontext(obj)==nontext(base)
 ns=sorted(b.sizes(str(obj)));assert ns==sorted(c['functions'])
 actual={n:list(b.verdict(*b.body(b.BLOB,n),*b.body(str(obj),n))) for n in c['verdicts']};assert actual==c['verdicts']
 changes=[n for n in ns if b.body(str(base),n)!=b.body(str(obj),n)];assert changes==c.get('changed_bodies',[])
 assert set(changes)<={'FPM_MTD_detect'} and not c.get('losses')
 audits.append({'cell':label,'verdicts':actual,'changed':changes,'metadata_nontext_identical':True})
assert base.read_bytes()==(run/'fpm_mtd/retained.o').read_bytes()
output={'complete_TU_cells':2,'shared_body_verdicts':6,'stage_streams':30,'stage_scope':'selected valid single-stream dumps; GCC GCSE diagnostic repeats are not parsed','current_short_mask_conversion_birth':'01.rtl UID106','current_terminal_boolean_birth':'01.rtl UID140','return_initialization_hoist':'20.combine -> 21.ce2 UID117','return_low_byte_fusion':'27.flow2 -> 28.peephole2 UIDs230-232','candidate_guarded_conversion_loss':'19.life -> 20.combine UID125','candidate_adoption':'declined: guarded original conversion is removed rather than recovered','audits':audits,'records':records}
(d.ROOT/'build/stage-c-mtd-value-trace.json').write_text(json.dumps(output,indent=2)+'\n')
print('2 complete TU audits;6 body verdicts;30 stage streams;short assignment/boolean birth,ce2 hoist,peephole fusion,and combine conversion deletion assertions pass')
