#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'FPM_circ_dotp2');cells={'baseline':source}
 for carry in (False,True):
  f=fn.replace('const short *c = coeff;','const short *c = coeff;\n\tconst short *h = hist + widx;')
  f=f.replace('hist[i] * *c','*h-- * *c')
  f=f.replace('\tfor (i = (short)(taps - 1);', '\th '+('+= taps;' if carry else '= hist + taps - 1;')+'\n\tfor (i = (short)(taps - 1);')
  cells['cursor-carry-%d'%carry]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-circular-dot';d.SOURCE_PATHS=('src/dsp/fpm_div32.c',);d.variants=variants;d.main()
