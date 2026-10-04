#!/usr/bin/env python3
"""SDM compound feedback versus left-associated assignment."""
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'FPM_SDM_descrambler')
 old='*data = (unsigned short)\n\t\t\t((reg >> shift1) ^ in ^ (reg >> shift2));'
 assert old in fn
 q=fn.replace(old,'*data ^= (reg >> shift1) ^ (reg >> shift2);')
 cells={'baseline':source}
 for compound in [False,True]:
  for post in [False,True]:
   if not(compound or post):continue
   f=q if compound else fn
   if post:
    assert '*data &= mask;\n\t\tdata++;' in f
    f=f.replace('*data &= mask;\n\t\tdata++;','*data++ &= mask;')
   cells['compound-%d-post-%d'%(compound,post)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-sdm-postincrement';d.SOURCE_PATHS=('src/dsp/fpm_sdm.c',);d.variants=variants;d.main()
