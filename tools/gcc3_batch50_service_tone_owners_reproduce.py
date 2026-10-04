#!/usr/bin/env python3
"""Inline floating detector coefficient/state pointer owner cross."""
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'TONE_detect');cells={'baseline':source}
 for coeff in [False,True]:
  for state in [False,True]:
   if not(coeff or state):continue
   f=fn
   if coeff:
    f=f.replace('t->det_coef[','det_coef[')
    f=f.replace('{\n','{\n\tconst float *det_coef = t->det_coef;\n',1)
   if state:
    f=f.replace('t->det_z1','det_state[0]').replace('t->det_z2','det_state[1]')
    f=f.replace('{\n','{\n\tfloat *det_state = &t->det_z1;\n',1)
   cells['coeff-%d-state-%d'%(coeff,state)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-tone-owners';d.SOURCE_PATHS=('src/service/TONE.c',);d.variants=variants;d.main()
