#!/usr/bin/env python3
"""Assert source next-state placement and GCC3 late call merging, four methods."""
import json,re
import playbook_small_patterns as d
out=d.ROOT/'build/gcc3-batch100-v90-initiate-state';records=[]
for family in ('V90Modulator','V92Modulator'):
 for label in ('baseline','state-before-diagnostic'):
  root=out/family/label
  for method in ('initiateFPE','initiateRRN'):
   stage_bodies={}
   for stage in ('01.rtl','25.greg','26.postreload','27.flow2','35.mach'):
    text=(root/(family+'.cpp.'+stage)).read_text()
    chunks=[s for s in re.split(r'^;; Function ',text,flags=re.M)[1:] if s.startswith('int '+family+'::'+method+'()')]
    assert len(chunks)==1;body=chunks[0];stage_bodies[stage]=body
    sites=body.count('("edprintf")');expected=1 if label=='state-before-diagnostic' and stage in ('27.flow2','35.mach') else 2
    assert sites==expected,(family,label,method,stage,sites)
    records.append({'family':family,'label':label,'method':method,'stage':stage,'edprintf_sites':sites})
   local='p4state' if family=='V90Modulator' else 'state'
   initial=stage_bodies['01.rtl'];sets=list(re.finditer(r'\(set \(reg/v:SI \d+ \[ '+local+r' \]\)\s*\(const_int \d+',initial));calls=list(re.finditer(r'\(call \(mem:QI \(symbol_ref:SI \("edprintf"\)',initial))
   assert len(sets)==len(calls)==2
   assert all((s.start()<c.start())==(label=='state-before-diagnostic') for s,c in zip(sets,calls)),(family,label,method,'source lifetime')
(out/'call-sites-stages.json').write_text(json.dumps({'records':records,'checked_stages':len(records),'initial_source_controls':8},indent=2)+'\n')
print('40/40 stage controls; eight initial source lifetimes; only winners merge two calls to one at 27.flow2')
