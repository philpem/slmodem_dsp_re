#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 cells={'baseline':source}
 for cursor,word,formal in itertools.product([False,True],repeat=3):
  if not(cursor or word or formal):continue
  s=source
  if cursor:
   a,z,f=d.function(s,'shell_correlate')
   f=f.replace('int lo = 0;','const short *lo = t;\n\tconst short *hi = t + d;')
   f=f.replace('t[lo] * (unsigned short)t[d]','*lo++ * (unsigned short)*hi--')
   f=f.replace('\n\t\tlo++;\n\t\td--;','')
   s=s[:a]+f+s[z:]
  a,z,f=d.function(s,'shell_group')
  if word:f=f.replace('int c, d;','unsigned short c, d;')
  if formal:f=f.replace('const short *sub, int n,','const short *sub, unsigned short n,')
  s=s[:a]+f+s[z:]
  cells['cursor-%d-word-%d-formal-%d'%(cursor,word,formal)]=s
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-shell-correlation';d.SOURCE_PATHS=('src/pump/v34/v34shell.c',);d.variants=variants;d.main()
