#!/usr/bin/env python3
"""Spectral shaping cached coefficient width x positive progress guard."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path, source):
 cells={}
 for narrow in (0,1):
  for guard in (0,1):
   text=source
   if narrow:
    for names in ('b0, b1, b2, b3','a0, a1, a2, a3'):
     old='long double '+names+';';assert text.count(old)==1
     text=text.replace(old,'float '+names+';')
   if guard:
    a,z,fn=driver.function(text,'V90SpectralShapingFilter::progress')
    old='\tif (left == 0)\n\t\treturn;';assert fn.count(old)==1
    fn=fn.replace(old,'\tif (left > 0) {');fn=fn[:-1]+'\t}\n}'
    text=text[:a]+fn+text[z:]
   cells['baseline' if not(narrow or guard) else f'float-coeff-{narrow}-positive-guard-{guard}']=text
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-ssf-coefficient';driver.SOURCE_PATHS=('src/pump/v90/V90SpectralShapingFilter.cpp',);driver.variants=variants;driver.main()
