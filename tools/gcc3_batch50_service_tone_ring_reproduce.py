#!/usr/bin/env python3
"""Inline floating detector coefficient/state pointer owner cross."""
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'TONE_detect')
 fn=fn.replace('t->det_coef[','det_coef[').replace('t->det_z1','det_state[0]').replace('t->det_z2','det_state[1]')
 fn=fn.replace('{\n','{\n\tconst float *det_coef = t->det_coef;\n\tfloat *det_state = &t->det_z1;\n',1)
 seed=source[:a]+fn+source[z:];cells={'baseline':source,'owners-only':seed}
 for narrow in [False,True]:
  for cursor in [False,True]:
   if not(narrow or cursor):continue
   text=seed
   for name in ['TONE_detect','TONE_filter']:
    a,z,f=d.function(text,name)
    if narrow:
     old='idx = (short)((short)(idx + 1) < len ? (short)(idx + 1) : 0);'
     assert old in f
     f=f.replace(old,'idx++;\n\t\tidx = idx < len ? idx : 0;')
    if cursor:
     f=f.replace('int j;','int j;\n\t\tconst float *h;')
     f=f.replace('for (j = idx; j >= 0; j--)','h = dly + idx;\n\t\tfor (j = idx; j >= 0; j--)').replace('for (j = len - 1; j > idx; j--)','h = dly + len - 1;\n\t\tfor (j = len - 1; j > idx; j--)').replace('*c++ * dly[j]','*c++ * *h--')
    text=text[:a]+f+text[z:]
   cells['narrow-%d-cursor-%d'%(narrow,cursor)]=text
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-tone-ring';d.SOURCE_PATHS=('src/service/TONE.c',);d.variants=variants;d.main()
