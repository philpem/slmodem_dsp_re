#!/usr/bin/env python3
"""Quality monitor original post-diagnostic owner and final count reads."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V17r_int.c','src/fax/V27r_int.c');d.OUT_NAME='batch100-fax-quality-owner'
def variants(path,s):
 n=path.split('/')[-1][1:3];a,z,f=d.function(s,'QualityDetectV'+n);cells={}
 for owner,count in product((False,True),repeat=2):
  x=f
  if owner:
   if n=='17':
    old='''if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V17 Dec error too big..." " unreliable data\\n");'''
    new=old.replace('if (DSPLIB_DEBUG_ON())','if (DSPLIB_DEBUG_ON()) {')+'\n\t\t\trx = RXSTATE(modem);\n\t\t}'
   else:
    old='''if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27 Dec error too big..." " unreliable data\\n");'''
    new=old.replace('if (DSPLIB_DEBUG_ON())','if (DSPLIB_DEBUG_ON()) {')+'\n\t\t\trx = ((struct v27_rx *)modem)->rx;\n\t\t}'
   assert old in x;x=x.replace(old,new)
  if count:
   if n=='17':x=x.replace('rx->qcount = (short)(n + 1);','rx->qcount = (short)(rx->qcount + 1);')
   else:x=x.replace('rx->q_count = (unsigned short)(n + 1);','rx->q_count = (unsigned short)(rx->q_count + 1);')
  l='-'.join(nm for nm,v in [('post-debug-owner',owner),('member-count',count)]if v)or'baseline';cells[l]=s[:a]+x+s[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
