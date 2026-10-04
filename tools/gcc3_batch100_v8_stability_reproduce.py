#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,original=d.function(source,'checkSignalStability');cells={}
 old='\tif (r->gain_ref == 0)\n\t\tdelta = 0;\n\telse\n\t\tdelta = (short)((((int)r->gain - r->gain_ref) << 14) / r->gain_ref);'
 for word in (0,1):
  for unguarded in (0,1):
   f=original
   if word:
    assert f.count('\tint delta;')==1;f=f.replace('\tint delta;','\tshort delta;')
   if unguarded:
    assert f.count(old)==1;f=f.replace(old,'\tdelta = (short)((((int)r->gain - r->gain_ref) << 14) / r->gain_ref);')
   name='baseline' if not(word or unguarded) else 'word-%d-unguarded-%d'%(word,unguarded)
   cells[name]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-v8-stability';d.SOURCE_PATHS=('src/v8/V8global.c',);d.variants=variants;d.main()
