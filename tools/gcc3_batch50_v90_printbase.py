#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/pump/v90/V90MP.cpp',);d.OUT_NAME='gcc3-batch50-v90-printbase'
def variants(path,source):
 cells={}
 for latch,test in itertools.product((False,True),repeat=2):
  start,end,fn=d.function(source,'V90MP::PrintBase2')
  if latch:fn=fn.replace('while (mask != 0) {','for (; mask != 0; mask >>= 1) {').replace('\t\tmask >>= 1;\n','')
  if test:fn=fn.replace('(value & mask)','(mask & value)')
  label='-'.join(n for n,v in [('for-latch',latch),('mask-left',test)] if v) or 'baseline';cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
