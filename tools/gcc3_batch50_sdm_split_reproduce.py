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
 cells={'baseline':source,'compound-post':source[:a]+q.replace('*data &= mask;\n\t\tdata++;','*data++ &= mask;')+source[z:]}
 for post in [False,True]:
  f=fn.replace(old,'*data ^= reg >> shift1;\n\t\t*data ^= reg >> shift2;')
  if post:f=f.replace('*data &= mask;\n\t\tdata++;','*data++ &= mask;')
  cells['split-post-%d'%post]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-sdm-split';d.SOURCE_PATHS=('src/dsp/fpm_sdm.c',);d.variants=variants;d.main()
