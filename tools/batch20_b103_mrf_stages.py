#!/usr/bin/env python3
"""Locate untouched B103 Answer first-pass change in the consistent MRF cross."""
import difflib,json,re
import playbook_small_patterns as d
p=d.ROOT/'build/batch20-b103-state/B103prc/org-ans';q=d.ROOT/'build/batch20-mrf-consumers-validated/B103prc/combined'
def normalized(text):
 text=text.split(';; Function B103AnswerNextState',1)[1].split(';; Function ',1)[0]
 text=re.sub(r'0x[0-9a-f]{7,}','POINTER',text)
 text=re.sub(r'\[\d+ (?=[^\]]* S\d+ A\d+\])','[ALIAS ',text)
 text=re.sub(r'\("/work/[^"\n]+"\) \d+','(FILE) LINE',text)
 return re.sub(r'/work/[^\s:]+:\d+','FILE:LINE',text)
reports={}
for a in sorted(p.glob('B103prc.c.*')):
 b=q/a.name
 if not b.exists() or '.00.' in a.name:continue
 x=normalized(a.read_text());y=normalized(b.read_text());reports[a.name]={'equal':x==y,'diff':list(difflib.unified_diff(x.splitlines(),y.splitlines())) if x!=y else []}
changed=[n for n,r in reports.items() if not r['equal']]
assert changed[0]=='B103prc.c.28.peephole2',changed[0]
assert reports['B103prc.c.27.flow2']['equal']
(d.ROOT/'build/batch20-b103-mrf-stages.json').write_text(json.dumps(reports,indent=2)+'\n')
print('B103 Answer:',len(reports),'passes; equal through27.flow2; first difference28.peephole2 (dx/cx and ax/dx scratch choices).')
