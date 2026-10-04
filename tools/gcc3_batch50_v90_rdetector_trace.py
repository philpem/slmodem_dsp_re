#!/usr/bin/env python3
"""Retain the first-CSE member-load distinction in four complete-TU controls."""
import json,re,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/gcc3-batch50-v90-rdetector/V90RDetector'
reports={}
for cell in ('baseline','group-guard','member-update','member-update-group-guard'):
 for stage in ('01.rtl','06.cse'):
  path=BASE/cell/('V90RDetector.cpp.'+stage);text=path.read_text()
  marker=re.search(r'^;; Function .*V90RDetector::detectRNot\(',text,re.M)
  assert marker is not None
  end=text.find(';; Function ',marker.end());body=text[marker.start():end if end>=0 else None]
  references=body.count('const_int 32 [0x20]')
  stores=len(re.findall(r'\(set\s+\(mem/s:HI\s+\(plus:SI\s+\(reg/v/u/f:SI \d+ \[ this \]\)\s+\(const_int 32 \[0x20\]\)',body))
  reports[cell+'/'+stage]={'dump_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'member_accesses':references,'stores':stores,'loads':references-stores,'function_rtl':body}
assert reports['baseline/01.rtl']['loads']==2
assert reports['baseline/06.cse']['loads']==1
assert reports['member-update-group-guard/06.cse']['loads']==2
out=ROOT/'build/batch50-v90-rdetector-cse-witness.json'
out.write_text(json.dumps({'revision':'902f47fa','function':'V90RDetector::detectRNot','snapshots':len(reports),'reports':reports},indent=2)+'\n')
print('8 function/pass snapshots; baseline loads2->1, crossed winner retains2 after firstCSE; complete RTL snippets retained')
