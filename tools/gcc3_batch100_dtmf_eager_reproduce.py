#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,original=d.function(source,'dtmf_test')
 cells={}
 for eager in (0,1):
  for early in (0,1):
   f=original
   if eager:
    assert f.count('&&')==4
    f=f.replace('&&','&')
   if early:
    for stmt in ('max_lo = 0.0f;','max_hi = 0.0f;','sum = 0.0f;'):
     assert f.count('\t'+stmt)==1
     f=f.replace('\t'+stmt+'\n','')
    marker='\tthreshold = (mode == DTMF_MODE_EUR) ? 0.0022f : 0.002f;'
    assert f.count(marker)==1
    f=f.replace(marker,'\tmax_lo = 0.0f;\n\tmax_hi = 0.0f;\n\tsum = 0.0f;\n\n'+marker)
   label='baseline' if not eager and not early else 'eager-%d-early-%d'%(eager,early)
   cells[label]=source[:a]+f+source[z:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-dtmf-eager';d.SOURCE_PATHS=('src/service/Dtmf.c',);d.variants=variants;d.main()
