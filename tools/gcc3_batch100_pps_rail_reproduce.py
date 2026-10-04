#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_pps_capture_reproduce as owners

def variants(path,source):
 cells={}
 for owner,s in owners.variants(path,source).items():
  for full in (0,1):
   for wrap in (0,1):
    a,z,f=d.function(s,'pps_rail')
    if full:
     assert f.count('\tshort i;')==1;f=f.replace('\tshort i;','\tint i;').replace('i = (short)(taps - 1)','i = taps - 1')
    if wrap:
     assert f.count('\th = hist + taps - 1;')==1;f=f.replace('\th = hist + taps - 1;','\th += taps;')
    label='baseline' if owner=='baseline' and not(full or wrap) else owner+'-full-%d-wrap-%d'%(full,wrap)
    cells[label]=s[:a]+f+s[z:]
 assert len(set(cells.values()))==16
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-pps-rail';d.SOURCE_PATHS=('src/dsp/fpm_pps.c',);d.variants=variants;d.main()
