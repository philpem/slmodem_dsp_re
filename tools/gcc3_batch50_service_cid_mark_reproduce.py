#!/usr/bin/env python3
"""CID input cursor crossed with eager comparison combination."""
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'CID_MTD_detect');cells={'baseline':source}
 for cursor in [False,True]:
  for eager in [False,True]:
   if not(cursor or eager):continue
   f=fn
   if cursor:f=f.replace('samples[i]','*samples++')
   if eager:f=f.replace('wide > 150 && wide / 2 > narrow','(wide > 150) & (wide / 2 > narrow)')
   cells['cursor-%d-eager-%d'%(cursor,eager)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-cid-mark';d.SOURCE_PATHS=('src/service/Cidmtd.c',);d.variants=variants;d.main()
