#!/usr/bin/env python3
"""Complete-TU source gain proof, including five changed selector destinations."""
import json,sys,argparse,hashlib,copy
from collections import Counter
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
parser=argparse.ArgumentParser();parser.add_argument('--toolchain-root',default=str(d.ROOT/'tools/toolchain'));args=parser.parse_args()
sys.path.insert(0,args.toolchain_root)
import jumptable as j
helper=(d.ROOT/'tools/batch100_data_unit_audit.py').read_text();exec(helper[helper.index("s=(d.ROOT/"):helper.index('import sys\nreports=')])
root=d.ROOT/'build/batch100-fax-v29-next-flags/V29r_prc';paths={'baseline':root/'baseline/candidate.o','candidate':root/'byte-transition-flags/candidate.o','blob':d.b.BLOB}
proof={k:j.prove(str(p),'RxNextStateV29')for k,p in paths.items()};assert proof['candidate']['identity']==proof['blob']['identity']
base=canonical(paths['baseline']);new=canonical(paths['candidate'])
for k in ('records','objects','nontext','allocated_sizes'):assert base[k]==new[k],k
expected=copy.deepcopy(base['nontext_relocations']);a=proof['baseline'];z=proof['candidate'];assert a['table_section']==z['table_section'] and a['table_offset']==z['table_offset'] and a['count']==z['count']==5
for index,(old,target) in enumerate(zip(a['targets'],z['targets'])):
 slot=a['table_offset']+4*index;assert expected[a['table_section']][slot]==('R_386_32',('function','RxNextStateV29',old))
 expected[a['table_section']][slot]=('R_386_32',('function','RxNextStateV29',target))
assert expected==new['nontext_relocations']
assert Counter(map(repr,base['text_relocations']))==Counter(map(repr,new['text_relocations']))
changed=[];gains=[];losses=[];common=set(d.b.sizes(paths['baseline']))&set(d.b.sizes(d.b.BLOB))
for n in sorted(common):
 old=d.b.body(paths['baseline'],n);actual=d.b.body(paths['candidate'],n);orig=d.b.body(d.b.BLOB,n)
 if old!=actual:changed.append(n)
 before=d.b.verdict(*orig,*old);after=d.b.verdict(*orig,*actual)
 if before[0]!='EXACT' and after[0]=='EXACT':gains.append(n)
 if before[0]=='EXACT' and after[0]!='EXACT':losses.append(n)
assert set(changed)=={'RxHdxEpochDetV29','RxNextStateV29'} and gains==['RxHdxEpochDetV29','RxNextStateV29'] and not losses,(changed,gains,losses)
report={'revision':'856c1ecb','prover_sha256':hashlib.sha256(Path(j.__file__).read_bytes()).hexdigest(),'proofs':proof,'changed':changed,'gains_including_predecessor':gains,'new_gain':{'RxNextStateV29':467},'losses':losses,'whole_TU_functions':len(common),'expected_five_selector_relocations':True,'other_metadata_data_nontext_relocations_bystanders_unchanged':True,'object_sha256':hashlib.sha256(paths['candidate'].read_bytes()).hexdigest()}
(d.ROOT/'build/batch100-fax-v29-next-full-audit.json').write_text(json.dumps(report,indent=2)+'\n');print('V29 complete TU:',len(common),'functions; new strict EXACT467 gain, fixed epoch retained; five selector destinations strictly proven; no losses or other bystander/data changes')
