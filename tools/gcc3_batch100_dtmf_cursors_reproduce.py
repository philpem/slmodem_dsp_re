#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'DTMF_MTD_detect');cells={'baseline':source}
 for low,high in itertools.product([False,True],repeat=2):
  if not(low or high):continue
  f=fn
  for first,last,active in [(0,3,low),(4,7,high)]:
   if not active:continue
   opening='\t\tfor (j = %d; j <= %d; j++) {'%(first,last);start=f.index(opening);end=f.index('\n\t\t}',start)+len('\n\t\t}')
   chunk=f[start:end];chunk=chunk.replace('rx->tone_state[j]', 'state')
   chunk=chunk.replace('\n\n\t\t\tenergy[j]', '\n\n\t\t\tstate += 2;\n\t\t\tenergy[j]')
   replacement='\t\t{\n\t\tshort *state = rx->tone_state[%d];\n'%first+chunk+'\n\t\t}'
   f=f[:start]+replacement+f[end:]
  cells['low-%d-high-%d'%(low,high)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-dtmf-cursors';d.SOURCE_PATHS=('src/service/Dtmf_Detector.c',);d.variants=variants;d.main()
