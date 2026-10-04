#!/usr/bin/env python3
"""Original FDSP echo quiet reuse, peak owner and count signedness."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'EchoCanceler');cells={'baseline':source}
 for capture,owner,unsigned in itertools.product([False,True],repeat=3):
  if not(capture or owner or unsigned):continue
  f=fn
  if capture:
   f=f.replace('if (fabs(x) < peak)\n\t\t\tquiet++;','{\n\t\tint low = fabs(x) < peak;\n\t\tif (low)\n\t\t\tquiet++;')
   f=f.replace('mu != 0.0f && fabs(x) < peak','mu != 0.0f && low')
   f=f.replace('\n\t}\n\t*verdict','\n\t\t}\n\t}\n\t*verdict')
  if owner:
   f=f.replace('peak = FDSP_FABS(hist[pos]);','{\n\t\tconst float *h = hist + pos;\n\t\tpeak = FDSP_FABS(h[0]);').replace('FDSP_FABS(hist[pos + k])','FDSP_FABS(h[k])')
   f=f.replace('\n\t\tpeak = peak * 0.5f;','\n\t\t}\n\t\tpeak = peak * 0.5f;')
  if unsigned:f=f.replace('int quiet = 0;','unsigned int quiet = 0;')
  cells['capture-%d-owner-%d-unsigned-%d'%(capture,owner,unsigned)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-fdsp-echo';d.SOURCE_PATHS=('src/service/Fdspkrnl.c',);d.variants=variants;d.main()
