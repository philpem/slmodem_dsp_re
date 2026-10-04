#!/usr/bin/env python3
"""Tone IIR observed expanded sections x narrowed tap loop counters."""
import re,sys
from pathlib import Path
import playbook_small_patterns as driver

def expanded(fn):
 marker='for (s = 0; s < IIR_FILTER_SECTIONS; s++) {';assert fn.count(marker)==1
 a=fn.index(marker);brace=fn.index('{',a);depth=1;i=brace+1
 while depth:
  if fn[i]=='{':depth+=1
  elif fn[i]=='}':depth-=1
  i+=1
 block=fn[brace:i]
 replacement='\n\t'.join(re.sub(r'\bs\b',str(s),block) for s in range(4))
 fn=fn[:a]+replacement+fn[i:]
 assert fn.count('int s;')==1
 return fn.replace('int s;','/* Four original sections below. */')

def variants(path,source):
 cells={}
 for narrow in (0,1):
  for unroll in (0,1):
   text=source
   for name in ('toneiir_progress','_iir_filter_progress'):
    a,z,fn=driver.function(text,name)
    if narrow:assert fn.count('int k;')==1;fn=fn.replace('int k;','short k;')
    if unroll:fn=expanded(fn)
    text=text[:a]+fn+text[z:]
   cells['baseline' if not(narrow or unroll) else f'short-taps-{narrow}-expanded-sections-{unroll}']=text
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-toneiir-sections';driver.SOURCE_PATHS=('src/callprog/toneiir.c',);driver.variants=variants;driver.main()
