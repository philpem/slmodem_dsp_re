#!/usr/bin/env python3
"""Pinned pool census; all duplicate defining copies retain their verdicts."""
import json
from pathlib import Path
import playbook_small_patterns as d
root=d.ROOT;b=d.b
rows=[];eligible=0
for obj in sorted((root/'build/tc_out').glob('*.o')):
 name=obj.name
 if not ((name.startswith('src_pump_v90_') or name.startswith('src_dsp_')) and '.cpp.o' in name):continue
 if 'V90Demodulator' in name:continue
 for symbol,ours in b.sizes(str(obj)).items():
  if symbol not in b.sizes(b.BLOB):continue
  eligible+=1;grade=b.verdict(*b.body(b.BLOB,symbol),*b.body(str(obj),symbol))
  if grade[0]!='EXACT':rows.append({'object':name,'symbol':symbol,'ours':ours,'blob':b.sizes(b.BLOB)[symbol],'verdict':grade})
 result={'revision':'856c1ecb','eligible_defining_copies':eligible,'nonexact_defining_copies':len(rows),'nonexact_names':len({r['symbol'] for r in rows}),'rows':rows}
(root/'build/batch100-v90-census.json').write_text(json.dumps(result,indent=2)+'\n')
print(eligible,'eligible defining copies;',len(rows),'nonexact copies;',result['nonexact_names'],'nonexact names')
