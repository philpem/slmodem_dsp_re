#!/usr/bin/env python3
"""Complete eligible owned completed-fax/data function census, baseline856c1ecb."""
import json
from collections import Counter
import playbook_small_patterns as d
roots=('src/fax','src/pump/b103','src/pump/v22','src/pump/v23','src/pump/v32')
b=d.b;bs=b.sizes(b.BLOB);rows=[];counts=Counter();tus=0;functions=0
for root in roots:
 for src in sorted((d.ROOT/root).glob('**/*.c')):
  relative=str(src.relative_to(d.ROOT));obj=d.ROOT/'build/tc_out'/(relative.replace('/','_')+'.o')
  if not obj.exists():continue
  tus+=1
  for name,size in b.sizes(str(obj)).items():
   if name not in bs:continue
   functions+=1;v=b.verdict(*b.body(b.BLOB,name),*b.body(str(obj),name));counts[v[0]]+=1
   if v[0]=='EXACT':continue
   rows.append({'name':name,'source':relative,'object':str(obj),'blob':bs[name],'ours':size,'verdict':v[0],'difference':v[1],'grade1':b.alpha_why(b.insns(b.BLOB,name),b.insns(str(obj),name)) is None if max(size,bs[name])<=1500 else None})
rows.sort(key=lambda r:(r['blob'],r['source'],r['name']))
report={'revision':'856c1ecb','roots':roots,'tus':tus,'functions':functions,'counts':dict(counts),'nonexact':len(rows),'rows':rows}
(d.ROOT/'build/batch100-data-screen.json').write_text(json.dumps(report,indent=2)+'\n')
print(tus,'eligible complete TUs;',functions,'common functions;',dict(counts),'nonexact',len(rows));print('nonexact by size',dict(Counter('<=240' if r['blob']<=240 else '<=650' if r['blob']<=650 else '<=1500' if r['blob']<=1500 else '>1500' for r in rows)));print('grade1 equal',sum(r['grade1'] is True for r in rows))
