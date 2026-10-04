#!/usr/bin/env python3
"""Prove all 300 final TUs equal either archived baseline or audited winners."""
import argparse,hashlib,json
from pathlib import Path
import playbook_small_patterns as d

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--inputs',type=Path,default=d.ROOT/'build/batch50-composition-inputs.json')
 args=ap.parse_args();spec=json.loads(args.inputs.read_text());before=d.ROOT/'build/production-before';after=d.ROOT/'build/tc_out'
 assert (before/'.build-config').read_bytes()==(after/'.build-config').read_bytes()
 manifest=(before/'tc_manifest.txt').read_text().splitlines();assert len(manifest)==300
 assert manifest==(after/'tc_manifest.txt').read_text().splitlines()
 winners=spec['winners'];reports={};changed=[]
 for line in manifest:
  obj,source=line.split();old=before/obj;new=after/obj
  expected=Path(winners[source]) if source in winners else old
  assert new.read_bytes()==expected.read_bytes(),(source,'not audited full-TU winner/baseline')
  raw_changed=new.read_bytes()!=old.read_bytes()
  if raw_changed:changed.append(source)
  reports[source]={'raw_changed':raw_changed,'sha256':hashlib.sha256(new.read_bytes()).hexdigest(),'expected':str(expected)}
 assert len(winners)==31,(len(winners),winners.keys())
 assert set(changed)<=set(winners),(set(changed)-set(winners))
 a=json.loads((d.ROOT/'build/after-byteident.json').read_text());z=json.loads((d.ROOT/'build/before-byteident.json').read_text())
 gains=sorted(set(a['exact_symbols'])-set(z['exact_symbols']));losses=sorted(set(z['exact_symbols'])-set(a['exact_symbols']))
 assert len(gains)==47 and not losses and a['unresolved']==z['unresolved']==5
 assert a['compared']==z['compared']==1852 and a['exact_bytes']-z['exact_bytes']==4810
 result={'objects':300,'winner_tus':len(winners),'raw_changed':len(changed),'gains':gains,'losses':losses,'exact_bytes_gain':4810,'before':z['exact'],'after':a['exact'],'reports':reports}
 (d.ROOT/'build/batch50-composition-audit.json').write_text(json.dumps(result,indent=2)+'\n')
 print('300/300 full objects match independent audited winners or immutable baseline;31 winner TUs; '+str(len(changed))+' raw changed')
 print('47 gains,4810B,0 losses; strict worst-copy census1020/1852;5 unresolved unchanged')
if __name__=='__main__':main()
