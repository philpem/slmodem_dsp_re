#!/usr/bin/env python3
"""Audit complete service replay objects, not only selected function scores."""
import json, hashlib
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

def main():
 summary={'cells':0,'body_comparisons':0,'gains':{},'losses':[],'families':{}}
 for package in ('services-v23','services-v23-post','services-cid-reset','services-fdsp-init','services-dtmf-predicate','services-dtmf-mask'):
  base=d.ROOT/'build'/package
  assert (base/'results.json').is_file(), ('missing required package', package)
  result=json.loads((base/'results.json').read_text())
  for name,f in result['families'].items():
   p=base/name;original=inspect(p/'baseline/candidate.o'); bodies={n:d.b.body(str(p/'baseline/candidate.o'),n) for n in f['cells']['baseline']['functions']}
   assert f['cells']['baseline']['baseline_reproduced']
   rows={}
   for label,cell in f['cells'].items():
    obj=p/label/'candidate.o';current=inspect(obj)
    assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash'],(package,label,'object hash')
    for symbol, verdict in cell['verdicts'].items():
     assert list(d.b.verdict(*d.b.body(d.b.BLOB,symbol),*d.b.body(str(obj),symbol)))==verdict,(package,label,symbol,'live verdict')
    for key in ('records','allocated','nobits','relocations'):assert current[key]==original[key],(package,label,key)
    now={n:d.b.body(str(obj),n) for n in bodies}
    changes=sorted(n for n in bodies if now[n]!=bodies[n])
    assert changes==sorted(cell.get('changed_bodies',[])),(package,label,changes)
    assert not cell.get('losses',[]),(package,label,'exact losses')
    assert cell.get('gains',[])==[n for n in cell['verdicts'] if cell['verdicts'][n][0]=='EXACT' and f['cells']['baseline']['verdicts'][n][0]!='EXACT']
    summary['cells']+=1;summary['body_comparisons']+=len(bodies)
    rows[label]={'changed':changes,'gains':cell.get('gains',[])}
    if cell.get('gains'):summary['gains'][package+'/'+label]=cell['gains']
   summary['families'][package+'/'+name]=rows
 # This positive witness proves the analysis sees a changed exact function.
 assert summary['gains']['services-cid-reset/common-after']==['cid_reset']
 assert (d.ROOT/'build/services-v23/v23tx/zero-1-mute-1-unset-1/candidate.o').read_bytes()==(d.ROOT/'build/services-v23-post/v23tx/boundary-control/candidate.o').read_bytes()
 summary['repeated_v23_control_raw_equal']=True
 assert (d.ROOT/'build/services-dtmf-predicate/Dtmf/eager-0-early-1/candidate.o').read_bytes()==(d.ROOT/'build/services-dtmf-mask/Dtmf/early-control/candidate.o').read_bytes()
 summary['repeated_dtmf_control_raw_equal']=True
 out=d.ROOT/'build/services-exact-audit.json';out.write_text(json.dumps(summary,indent=2)+'\n')
 assert summary['cells']==28 and summary['body_comparisons']==125,summary
 print('Services audit: 28 cells, 125 emitted bodies; cid_reset gain, zero losses; two raw repeats; metadata/data preserved')
if __name__=='__main__':main()
