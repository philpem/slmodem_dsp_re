#!/usr/bin/env python3
"""Audit declared full-TU C++ graph controls, with body-change positive control."""
import json, hashlib
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
PACKAGES=('segment','precoder','sd','bitreset')
def main():
 report={};denominator=0
 for package in PACKAGES:
  root=d.ROOT/'build'/('batch-cpp-'+package)
  x=json.loads((root/'results.json').read_text());pr={}
  for family,fr in x['families'].items():
   base=root/family/'baseline/candidate.o';bi=inspect(base)
   retained=root/family/'retained.o';assert base.read_bytes()==retained.read_bytes(),package
   cells={};sizes=d.b.sizes(str(base));reference=d.b.sizes(d.b.BLOB)
   for label,c in fr['cells'].items():
    obj=root/family/label/'candidate.o';ci=inspect(obj)
    assert hashlib.sha256(obj.read_bytes()).hexdigest()==c['object_hash'],(package,label,'object hash')
    for symbol, verdict in c['verdicts'].items():
     assert list(d.b.verdict(*d.b.body(d.b.BLOB,symbol),*d.b.body(str(obj),symbol)))==verdict,(package,label,symbol,'live verdict')
    assert not c.get('losses'),(package,label,'exact loss')
    checks={key:bi[key]==ci[key] for key in ('records','allocated','nobits','relocations')}
    assert all(checks.values()),(package,label,checks)
    bodies={n:d.b.body(str(base),n)==d.b.body(str(obj),n) for n in sizes}
    changed=[n for n,equal in bodies.items() if not equal];assert sorted(changed)==sorted(c.get('changed_bodies',[]) ),(package,label,changed,c.get('changed_bodies'))
    denominator+=len(bodies)
    cells[label]={'invariants':checks,'bodies_compared':len(bodies),'changed':changed,'gains':c.get('gains',[]),'losses':c.get('losses',[]),'sizes':{n:d.b.sizes(str(obj))[n] for n in changed}}
   pr[family]=cells
  report[package]=pr
 # Known real changed body: apparatus must identify its denominator and fire.
 control=report['bitreset']['V90bitsToSymbol']['conditional']
 assert control['changed']==['_ZN15V90BitsToSymbol5resetEP16V90MappingParams7PcmType']
 assert len(control['gains'])==1 and not control['losses']
 report['denominator']=denominator
 out=d.ROOT/'build/batch-cpp-audit.json';out.write_text(json.dumps(report,indent=2)+'\n')
 print('C++ full-TU audit: %d emitted body comparisons, 4 raw baselines reproduced, metadata/data/BSS/relocations passed; positive body-change control fired1/1.'%denominator)
if __name__=='__main__':main()
