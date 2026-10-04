#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'V8GetMessage');cells={'baseline':source}
 for guard,unsigned in itertools.product([False,True],repeat=2):
  if not(guard or unsigned):continue
  f=fn
  if unsigned:f=f.replace('seq->word[i] >> 1','(unsigned short)seq->word[i] >> 1')
  if guard:
   f=f.replace('int n = seq->wordidx;','short received = seq->wordidx;').replace('int rc = 0;','int rc = V8_GET_EMPTY;')
   f=f.replace('if (n <= 0)\n\t\treturn V8_GET_EMPTY;','if (received > 0) {\n\t\tint n = received;\n\t\trc = 0;')
   f=f.replace('\treturn rc;','\t}\n\treturn rc;')
  cells['guard-%d-unsigned-%d'%(guard,unsigned)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-v8-message';d.SOURCE_PATHS=('src/v8/V8Interface.c',);d.variants=variants;d.main()
