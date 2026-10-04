#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_shell_init_reproduce as prior
def variants(path,source):
 s=prior.variants(path,source)['word-1-early-1-index-1'];a,z,fn=d.function(s,'initG248');cells={'baseline':source}
 for first,second in itertools.product([False,True],repeat=2):
  f=fn
  if first:
   f=f.replace('for (i = 0; i < n; i++) {','if (n > 0) {\n\t\ti = 0;\n\t\tdo {')
   f=f.replace('\t\ts->t1[i] = (short)(i + 1);\n\t}', '\t\ts->t1[i] = (short)(i + 1);\n\t\t} while (++i < n);\n\t}')
  if second:
   f=f.replace('for (j = 0; j <= top; j++) {','j = 0;\n\tdo {')
   f=f.replace('\t\ts->t2[j] = (short)acc;\n\t}', '\t\ts->t2[j] = (short)acc;\n\t} while (++j <= top);')
  cells['first-%d-second-%d'%(first,second)]=s[:a]+f+s[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-shell-init-cfg';d.SOURCE_PATHS=('src/pump/v34/v34shell.c',);d.variants=variants;d.main()
