#!/usr/bin/env python3
"""FIFO read independent original data and padding postdecrements."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/fifo.c',);d.OUT_NAME='batch100-fax-fifo-read'
def variants(path,s):
 a,z,f=d.function(s,'FIFO_read');cells={}
 for data,pad in product((False,True),repeat=2):
  x=f
  if data:x=x.replace('for (i = take; i != 0; i--)','for (i = take; i-- != 0;)')
  if pad:x=x.replace('for (i = pad; i != 0; i--)','for (i = pad; i-- != 0;)')
  l='-'.join(n for n,v in [('data-postdec',data),('pad-postdec',pad)] if v) or 'baseline'
  cells[l]=s[:a]+x+s[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
