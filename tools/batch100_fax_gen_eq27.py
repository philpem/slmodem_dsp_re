#!/usr/bin/env python3
"""Original EQ initializer signed loop and postdecrement pointer walk."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V27t_prc.c',);d.OUT_NAME='batch100-fax-gen-eq27'
def variants(path,s):
 a,z,f=d.function(s,'GenEQTrnSequenceV27');cells={}
 for index,walk,reread in product((False,True),repeat=3):
  x=f
  if index:x=x.replace('unsigned short i;','short i;')
  if reread:x=x.replace('\n\t\tshort rate = prm->rate;','').replace('[rate]','[prm->rate]')
  if walk:
   old='''for (i = 0; i < count; i++) {
			if (buf[i + 1] & 0x04)'''
   new='''unsigned short n;
		unsigned short *p = buf;

		for (n = count; n-- != 0;) {
			unsigned short *q = p++;
			if (q[1] & 0x04)'''
   assert old in x;x=x.replace(old,new).replace('buf[i] = (unsigned short)','*q = (unsigned short)')
  l='-'.join(n for n,v in [('signed-fill-index',index),('postdec-walk',walk),('rate-reread',reread)]if v)or'baseline';cells[l]=s[:a]+x+s[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
