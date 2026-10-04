#!/usr/bin/env python3
"""Read-only mechanism inventory of immutable complete-TU period objects.

Categories are structural observations, not complete causal explanations.
Uses byteident's typed relocations, strict worst-copy grading and alpha proof.
"""
import argparse
import collections
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Avoid tools/dis.py shadowing Python's standard dis module.
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != ROOT/'tools']
import dis
sys.path.insert(0, str(ROOT/'tools/toolchain'))
import byteident as b
RANK={'EXACT':0,'UNRESOLVED':1,'REGALLOC':2,'RELOC':3,'BYTES':4,'SIZE':5,'NODATA':6}

def strip(rows):
 return [r for r in rows if not b._padding(*r)]

def dead_pop(x,y):
 """Prove only syntactically dead ECX/EDX POP destinations differ.

 No return-EAX assumption. No branches/calls or unknown instructions after
 differing pops; no occurrence of either register family on any suffix row.
 This establishes an operand candidate, not the compiler pass creating it.
 """
 x,y=strip(x),strip(y)
 if len(x)!=len(y):return []
 different=[i for i,(a,c) in enumerate(zip(x,y)) if a!=c]
 if not different:return []
 for i in different:
  a,c=x[i],y[i]
  if a[0]!='pop' or c[0]!='pop' or a[1] not in ('%ecx','%edx') or c[1] not in ('%ecx','%edx'):return []
  for rows in (x,y):
   suffix=rows[i+1:]
   if not suffix or suffix[-1]!=('ret',''):return []
   for mn,ops in suffix:
    if mn not in ('mov','pop','add','ret'):return []
    if any(b.REG32.get(reg) in ('ecx','edx') for reg in b.REGTOK.findall(ops)):return []
    if mn=='add' and not re.fullmatch(r'\$0x[0-9a-f]+,%esp',ops):return []
 return different

def shape(rows):
 return [(mn,b.REGTOK.sub(lambda m:'%'+str(len(m.group(0))),' '+ops).strip()) for mn,ops in strip(rows)]

def features(x,y):
 x,y=strip(x),strip(y)
 calls=lambda rs:collections.Counter(ops[ops.index('@'):] for mn,ops in rs if mn in ('call','lcall') and '@R_386_PC32:' in ops)
 ca,cb=calls(x),calls(y)
 jumps=lambda rs:collections.Counter(mn for mn,ops in rs if mn.startswith('j') or mn.startswith('loop'))
 return {'blob_instructions':len(x),'ours_instructions':len(y),
  'mnemonics_equal':[mn for mn,_ in x]==[mn for mn,_ in y],
  'register_erased_operands_equal':shape(x)==shape(y),
  'branch_mnemonics_blob':dict(jumps(x)),'branch_mnemonics_ours':dict(jumps(y)),
  'call_targets_blob':dict(ca),'call_targets_ours':dict(cb),
  'calls_only_blob':dict(ca-cb),'calls_only_ours':dict(cb-ca),
  'indirect_calls_blob':[ops for mn,ops in x if mn=='call' and ops.startswith('*')],
  'indirect_calls_ours':[ops for mn,ops in y if mn=='call' and ops.startswith('*')],
  'unrelocated_direct_calls_blob':[ops for mn,ops in x if mn=='call' and not ops.startswith('*') and '@' not in ops],
  'unrelocated_direct_calls_ours':[ops for mn,ops in y if mn=='call' and not ops.startswith('*') and '@' not in ops],
  'alpha_rejection':b.alpha_why(x,y),'dead_pop_rows':dead_pop(x,y),
  'unresolved_selector_present':any(mn=='jmp' and ops.startswith('*') and "('section'," in ops for mn,ops in x+y)}

def self_test():
 positives=0;negatives=0
 x=[('mov','$0x1,%eax'),('pop','%ecx'),('ret','')]
 y=[('mov','$0x1,%eax'),('pop','%edx'),('ret','')]
 assert dead_pop(x,y)==[1];positives+=1
 for z in ([('mov','$0x2,%eax'),('pop','%edx'),('ret','')],
           [('mov','$0x1,%eax'),('pop','%eax'),('ret','')],
           [('pop','%edx'),('mov','%edx,%eax'),('ret','')],
           [('pop','%edx'),('call','helper'),('ret','')]):
  assert not dead_pop(x,z);negatives+=1
 assert b.alpha_equal([('mov','$0x1,%eax'),('mov','%eax,(%esp)'),('ret','')],
                      [('mov','$0x1,%edx'),('mov','%edx,(%esp)'),('ret','')]);positives+=1
 assert not b.alpha_equal([('mov','$0x1,%eax'),('ret','')],[('mov','$0x2,%edx'),('ret','')]);negatives+=1
 assert not dead_pop([('pop','%ecx'),('mov','%ecx,%eax'),('ret','')], [('pop','%edx'),('mov','%ecx,%eax'),('ret','')]);negatives+=1
 same=features([('call',".+1 @R_386_PC32:('symbol', 'helper', 4294967292)")], [ ('call',".+101 @R_386_PC32:('symbol', 'helper', 4294967292)")])
 assert not same['calls_only_blob'] and not same['calls_only_ours'];positives+=1
 indirect=features([('call','*%eax')],[('call','*%edx')])
 assert not indirect['calls_only_blob'] and not indirect['calls_only_ours'];negatives+=1
 different=features([('call',".+1 @R_386_PC32:('symbol', 'first', 4294967292)")], [('call',".+1 @R_386_PC32:('symbol', 'second', 4294967292)")])
 assert len(different['calls_only_blob'])==len(different['calls_only_ours'])==1;positives+=1
 return {'positive':positives,'negative':negatives,'passed':True}

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--objects',type=Path,default=ROOT.parent/'byteexact-batch100/build/tc_out')
 p.add_argument('--blob',type=Path,default=ROOT/'ref/slmodemd/dsplibs.o')
 p.add_argument('--output',type=Path,default=ROOT/'build/gcc3-mechanism-inventory.json')
 p.add_argument('--expected-nonexact',type=int,default=811)
 p.add_argument('--self-test',action='store_true')
 args=p.parse_args();control=self_test()
 if args.self_test:
  print(json.dumps(control));return
 objects=sorted(args.objects.glob('*.o'));assert len(objects)==300,len(objects)
 copies=collections.defaultdict(list)
 for obj in objects:
  for name in b.sizes(str(obj)):copies[name].append(obj)
 blob=b.sizes(str(args.blob));common=sorted(set(blob)&set(copies));assert len(common)==1852,len(common)
 counts=collections.Counter();records=[];exact=0;copy_count=0
 for number,name in enumerate(common):
  ab,ar=b.body(str(args.blob),name);scored=[]
  for obj in copies[name]:
   bb,br=b.body(str(obj),name);v=b.verdict(ab,ar,bb,br)
   scored.append((v,obj,br,len(bb) if bb is not None else None))
  worst=max(scored,key=lambda c:RANK[c[0][0]])
  if worst[0][0]=='EXACT':exact+=1;continue
  x=b.insns(str(args.blob),name);details=[]
  for v,obj,br,size in scored:
   f=features(x,b.insns(str(obj),name));f.update({'object':str(obj),'grade':v[0],'distance':v[1],
     'size':size,'typed_relocations_equal':ar==br,
     'typed_relocation_identity_proved':ar==br and all(value[1][0]!='section' for value in list(ar.values())+list(br.values())),
     'blob_unresolved_relocations':[{'offset':off,'value':value} for off,value in ar.items() if value[1][0]=='section'],
     'unresolved_relocations':[{'offset':off,'value':value} for off,value in br.items() if value[1][0]=='section']})
   details.append(f);copy_count+=1
  failed=[f for f in details if f['grade']!='EXACT']
  if all(f['dead_pop_rows'] and f['typed_relocation_identity_proved'] and f['size']==len(ab) for f in failed):category='dead-scratch-pop-candidate'
  elif any(f['grade']=='UNRESOLVED' for f in details):category='unresolved-selector' if any(f['unresolved_selector_present'] for f in details) else 'unresolved-data-relocation'
  elif all(f['alpha_rejection'] is None and f['typed_relocation_identity_proved'] for f in failed):category='proved-alpha-register-only'
  elif any(f['calls_only_blob'] or f['calls_only_ours'] for f in failed):category='size-and-helper-call-candidate' if any(f['grade']=='SIZE' for f in failed) else 'helper-call-boundary-candidate'
  elif any(not f['mnemonics_equal'] for f in failed):category='instruction-sequence-difference'
  elif all(f['register_erased_operands_equal'] for f in failed):category='register-topology-conflict'
  else:category='nonregister-operand-or-relocation-difference'
  counts[category]+=1
  records.append({'symbol':name,'blob_size':len(ab),'worst_grade':worst[0][0],
   'worst_object':str(worst[1]),'category':category,'copies':details})
  if number%200==0:print('classified %d/%d common symbols'%(number+1,len(common)),flush=True)
 assert len(records)==args.expected_nonexact,(len(records),args.expected_nonexact)
 shortlist=sorted(records,key=lambda r:(0 if r['category']=='dead-scratch-pop-candidate' else 1 if r['category']=='proved-alpha-register-only' else 2 if r['category']=='nonregister-operand-or-relocation-difference' else 3,r['blob_size']))
 result={'objects':len(objects),'common_symbols':len(common),'exact':exact,'nonexact':len(records),
  'nonexact_emitted_copies':copy_count,'categories':dict(counts),'self_test':control,
  'source_changes':False,'object_hashes':{str(o):hashlib.sha256(o.read_bytes()).hexdigest() for o in objects},
  'blob_sha256':hashlib.sha256(args.blob.read_bytes()).hexdigest(),
  'build_config':(args.objects/'.build-config').read_text(),'records':records,
  'ranked_structural_candidates':[r['symbol'] for r in shortlist]}
 args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps({k:result[k] for k in ('objects','common_symbols','exact','nonexact','nonexact_emitted_copies','categories','self_test')}))
if __name__=='__main__':main()
