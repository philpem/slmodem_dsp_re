#!/usr/bin/env python3
"""Replay captured SI/general scratch eligibility; no compiler state modification."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/gcc3-mechanism-scratch'
def transfer(event,start):
 assert event['constraint']=='r' and event['mode']=='SImode','unsupported prediction domain'
 s=event['eligibility'];eligible=[]
 for reg in s['class_members']:
  if s['fixed'][reg] or not s['call_used'][reg] and not s['ever_live'][reg]:continue
  if reg in (6,20)and(not s['reload_completed']or s['frame_pointer_needed']):continue
  if reg in s['live']or reg in event['excluded']:continue
  assert 0<=reg<8,'unsupported SI register-mode proof'
  eligible.append(reg)
 for i in range(53):
  index=(start+i)%53;reg=event['allocation_order'][index]
  if reg in eligible:return reg,(index+1)%53
 return None,0
def validate(event):
 prediction=transfer(event,event['cursor_before']);assert prediction==(event['selected'],event['cursor_after'])
 return prediction
reports={};events=0;models=0
for filename in ['results-gcc3-mechanism-fdsp.json','results-gcc3-mechanism-fdsp-reduced.json']:
 source=json.loads((OUT/filename).read_text())
 for label,cell in source['cells'].items():
  rows=cell['trace']['events'];events+=len(rows);target=[]
  for e in rows:
   assert all(c['register']==e['allocation_order'][c['order_index']]for c in e['candidates'])
   if e['constraint']!='r'or e['mode']!='SImode':continue
   prediction=validate(e)
   models+=1
   if e['function']=='FDSP_DP_Run':target.append({'entry':e['cursor_before'],'selected':e['selected'],'exit':e['cursor_after'],'live':e['eligibility']['live'],'maps':[transfer(e,i)for i in range(53)]})
  reports[label]={'events':len(rows),'target':target}
assert models>0 and len(reports)==5
# Detector controls: unsupported modes and a corrupted selection must not pass.
wrong=dict(e);wrong['mode']='unsupported'
try:transfer(wrong,0)
except AssertionError:unsupported_refused=True
else:raise AssertionError('unsupported mode accepted')
corrupt=dict(e);corrupt['selected']=0 if e['selected']!=0 else 1
try:validate(corrupt)
except AssertionError:corrupt_refused=True
else:raise AssertionError('corrupted selected register accepted')
report={'corrupt_selection_refused':corrupt_refused,'observed_events':events,'modeled_events':models,'cells':reports,'unsupported_mode_refused':unsupported_refused,'source_identity':'9f1199b5','diagnostic_only':True}
(OUT/'model.json').write_text(json.dumps(report,indent=2)+'\n')
print(models,'/',events,'scratch choices reproduced;5 observational cells; unsupported mode refused')
for label,r in reports.items():print(label,[{k:v for k,v in t.items()if k!='maps'}for t in r['target']])
