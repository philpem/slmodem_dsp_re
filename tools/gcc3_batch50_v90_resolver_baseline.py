#!/usr/bin/env python3
"""Compare proof versions on explicitly archived902f47fa production objects."""
import importlib.util, json, sys, hashlib
from pathlib import Path
import playbook_small_patterns as d
import jumptable as current
b=d.b
root=d.ROOT
oldpath=root/'build/gcc3-batch50-v90-cycle/jumptable-before.py'
spec=importlib.util.spec_from_file_location('jumptable_before',oldpath)
old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
objects=sorted((root/'build/tc_out').glob('*.o'));assert len(objects)==300
owners={}
for path in objects:
 for name in b.sizes(str(path)):
  owners.setdefault(name,[]).append(str(path))
names=sorted(set(owners)&set(b.sizes(b.BLOB)));assert len(names)==1852
before=json.loads(Path('/tmp/slmodem-byteexact-batch50/build/before-byteident.json').read_text())
reports={}
for label,prover in [('historical',old),('fixed-frame',current)]:
 sys.modules['jumptable']=prover;b.body.cache_clear()
 exact=[];unproved=[]
 for name in names:
  a=b.body(b.BLOB,name);grades=[b.verdict(*a,*b.body(path,name))[0] for path in owners[name]]
  if all(grade=='EXACT' for grade in grades):exact.append(name)
  elif all(grade in ('EXACT','UNRESOLVED') for grade in grades):unproved.append(name)
 reports[label]={'exact_symbols':exact,'exact':len(exact),'unproved':unproved,'compared':len(names)}
 print(label,len(exact),'exact of',len(names),'unproved',unproved,flush=True)
assert reports['historical']['exact_symbols']==before['exact_symbols']
sys.modules['jumptable']=current;b.body.cache_clear()
a=set(reports['historical']['exact_symbols']);c=set(reports['fixed-frame']['exact_symbols'])
report={'revision':'902f47fa','scope':'immutable archived production objects, not current edited source','objects':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in objects},'prover_before_hash':hashlib.sha256(oldpath.read_bytes()).hexdigest(),'prover_after_hash':hashlib.sha256((root/'tools/toolchain/jumptable.py').read_bytes()).hexdigest(),'reports':reports,'apparatus_only_gains':sorted(c-a),'apparatus_only_losses':sorted(a-c)}
(root/'build/batch50-v90-resolver-archive-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print('apparatus-only gains',sorted(c-a),'losses',sorted(a-c))
