#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 cells={}
 for high in (0,1):
  for low in (0,1):
   text=source
   for enabled,name in ((high,'dMinHighRates'),(low,'dMinLowRates')):
    if enabled:
     old='\tfloat %s = nanf("");'%name
     assert text.count(old)==1; text=text.replace(old,'\tfloat %s = NAN;'%name)
   cells['baseline' if not(high or low) else 'high-%d-low-%d'%(high,low)]=text
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='f2cdb66a';d.OUT_NAME='gcc3-uref-designer-nan';d.SOURCE_PATHS=('src/pump/v90/V90ConstellationDesigner.cpp',);d.variants=variants;d.main()
