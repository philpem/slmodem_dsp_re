#!/usr/bin/env python3
"""Original IIR section cursor/counter/feedforward width cube."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'FPM_iir_filt');cells={'baseline':source}
 for cursor,wordcount,wordff in itertools.product([False,True],repeat=3):
  if not(cursor or wordcount or wordff):continue
  f=fn
  if wordcount:
   f=f.replace('int i;','').replace('for (i = 0; i < sections; i++)','while (sections--)')
  if wordff:f=f.replace('int ff;','short ff;')
  if cursor:
   f=f.replace('int w1 = state[0];','int w1 = *state;').replace('w2 = state[1];','w2 = state[1];').replace('state[0] = (short)w2;','*state++ = (short)w2;').replace('state[1] = (short)w;','*state++ = (short)w;')
   for i in range(5):f=f.replace('coeff['+str(i)+']','*coeff++')
   f=f.replace('\n\t\tcoeff += FPM_IIR_COEFF_PER_SECTION;\n\t\tstate += FPM_IIR_STATE_PER_SECTION;','')
  cells['cursor-%d-count-%d-ff-%d'%(cursor,wordcount,wordff)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-iir-sections';d.SOURCE_PATHS=('src/dsp/fpm_iir.c',);d.variants=variants;d.main()
