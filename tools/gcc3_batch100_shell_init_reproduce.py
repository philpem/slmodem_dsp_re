#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'initG248');cells={'baseline':source}
 for word,early,index in itertools.product([False,True],repeat=3):
  if not(word or early or index):continue
  f=fn
  if word:f=f.replace('unsigned n = (unsigned short)s->count;','unsigned short n = (unsigned short)s->count;')
  if early:
   f=f.replace('unsigned i, j, k, cursor, len;','unsigned i, j, k, len;\n\tunsigned cursor = (unsigned)xyz[n];')
   f=f.replace('\tcursor = (unsigned)xyz[n];\n','')
  if index:f=f.replace('const short *b = s->t1 + j;','const short *b = s->t1 + (unsigned short)j;')
  cells['word-%d-early-%d-index-%d'%(word,early,index)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-shell-init';d.SOURCE_PATHS=('src/pump/v34/v34shell.c',);d.variants=variants;d.main()
