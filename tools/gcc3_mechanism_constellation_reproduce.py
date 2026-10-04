#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 cells={}
 for ordinary in (0,1):
  for codec in (0,1):
   text=source
   for enabled,name in ((ordinary,'getConstellationMask'),(codec,'getCodecConstellationMask')):
    if enabled:
     a,z,f=d.function(text,name)
     assert f.count('\tint k, i;')==1;f=f.replace('\tint k, i;','\tint k;\n\tunsigned int i;')
     text=text[:a]+f+text[z:]
   cells['baseline' if not(ordinary or codec) else 'ordinary-%d-codec-%d'%(ordinary,codec)]=text
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='9f1199b5';d.OUT_NAME='gcc3-mechanism-constellation';d.SOURCE_PATHS=('src/pump/v90/V90MappingParamsInt.cpp',);d.variants=variants;d.main()
