#!/usr/bin/env python3
"""Recheck complete MTD cells, sign-mask stages and inert branch control."""
import argparse,hashlib,json
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text();exec(s[s.index('def nontext'):s.index('\nreports=')])
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir',type=Path,default=d.ROOT/'build')
build=parser.parse_args().build_dir
rows=[]
for run in ('stage-c-mtd-residual','stage-c-mtd-branches'):
 out=build/run;r=json.loads((out/'results.json').read_text());f=r['families']['fpm_mtd'];base=out/'fpm_mtd/baseline/candidate.o'
 for label,c in f['cells'].items():
  folder=out/'fpm_mtd'/label;obj=folder/'candidate.o'
  assert hashlib.sha256(obj.read_bytes()).hexdigest()==c['object_hash']
  assert inspect(base)==inspect(obj) and nontext(base)==nontext(obj)
  names=sorted(b.sizes(str(obj)));assert names==sorted(c['functions'])
  changes=[n for n in names if b.body(str(base),n)!=b.body(str(obj),n)]
  assert changes==c.get('changed_bodies',[])
  assert set(changes)<={'FPM_MTD_detect'}
  actual={n:list(b.verdict(*b.body(b.BLOB,n),*b.body(str(obj),n))) for n in c['verdicts']}
  assert actual==c['verdicts'] and not c.get('losses')
  if label=='baseline':assert obj.read_bytes()==(out/'fpm_mtd/retained.o').read_bytes()
  rtl=(folder/'fpm_mtd.c.01.rtl').read_text().split(';; Function FPM_MTD_detect',1)[1]
  if label.startswith('mask'):
   tail=rtl[rtl.index('(not:SI'):];assert '(ashiftrt:SI' in tail and '(const_int 15 [0xf])' in tail and '(and:SI' in tail
  rows.append({'run':run,'cell':label,'object_hash':c['object_hash'],'verdicts':actual,'changed':changes,'initial_explicit_sign_mask':label.startswith('mask'),'metadata_nontext_identical':True})
# Exhaustive short-domain equivalence is apparatus proof, not modem reachability.
checks=0
for raw in range(65536):
 value=raw if raw<32768 else raw-65536
 masked=value & (~value >>15)
 assert masked==max(value,0)
 checks+=1
assert len(rows)==8
assert (build/'stage-c-mtd-branches/fpm_mtd/baseline/candidate.o').read_bytes()==(build/'stage-c-mtd-branches/fpm_mtd/-branches/candidate.o').read_bytes()
assert (build/'stage-c-mtd-branches/fpm_mtd/mask/candidate.o').read_bytes()==(build/'stage-c-mtd-branches/fpm_mtd/mask-branches/candidate.o').read_bytes()
result={'cells':8,'shared_body_verdicts':24,'short_mask_equivalence_checks':checks,'explicit_terminal_branch_control':'raw-inert-with-and-without-mask','rows':rows}
(build/'stage-c-mtd-residual-audit.json').write_text(json.dumps(result,indent=2)+'\n')
print('8/8 complete-TU audits;24 body verdicts;2 raw unchanged controls;2 inert branch pairs;65536 short-mask equivalence checks')
