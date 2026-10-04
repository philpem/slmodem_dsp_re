#!/usr/bin/env python3
"""Denominator-reporting screen of root-owned completed-source leaves."""
import json
from pathlib import Path
import playbook_small_patterns as d
b=d.b;blob=b.sizes(b.BLOB);exact=set(json.loads((d.ROOT/'build/before-byteident.json').read_text())['exact_symbols'])
rows=[];compared=0;sources=0
for line in (d.ROOT/'build/production-before/tc_manifest.txt').read_text().splitlines():
 obj,source=line.split()
 if not source.startswith(('src/core/','src/service/','src/call/','src/callprog/','src/dialer/','src/voice/')):continue
 sources+=1;p=d.ROOT/'build/production-before'/obj
 for name,size in b.sizes(str(p)).items():
  if name not in blob:continue
  compared+=1
  if name in exact or blob[name]>2500:continue
  v=b.verdict(*b.body(b.BLOB,name),*b.body(str(p),name))
  rows.append(dict(source=source,object=obj,name=name,blob=blob[name],ours=size,verdict=v[0],gap=v[1]))
rows.sort(key=lambda r:(r['blob'],r['source'],r['name']))
(d.ROOT/'build/batch100-root-screen.json').write_text(json.dumps(dict(sources=sources,compared=compared,eligible=len(rows),rows=rows),indent=2)+'\n')
print(sources,'TUs,',compared,'shared functions,',len(rows),'nonexact bodies <=2500 bytes')
for r in rows:print(r['source'],r['name'],r['blob'],r['ours'],r['verdict'],r['gap'])
