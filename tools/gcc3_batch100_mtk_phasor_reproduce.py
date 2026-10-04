#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'MTK_phasor');cells={'baseline':source}
 for word,predicate,member in itertools.product([False,True],repeat=3):
  if not(word or predicate or member):continue
  f=fn
  if word:f=f.replace('int n;','short n;')
  if predicate:f=f.replace('if (n & 0x100)','if (quadrant & 1)')
  if member:f=f.replace('if (s >= 3.141592653589793)\n\t\ts = (float)(s - 6.28318530718);\n\tp->phase = s;','if (s >= 3.141592653589793)\n\t\tp->phase = s - 6.28318530718;\n\telse\n\t\tp->phase = s;')
  cells['word-%d-predicate-%d-member-%d'%(word,predicate,member)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-mtk-phasor';d.SOURCE_PATHS=('src/service/PHASOR.c',);d.variants=variants;d.main()
