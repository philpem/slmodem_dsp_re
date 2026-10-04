#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,original=d.function(source,'FPM_FSD_demodulate');cells={}
 for cfg in (0,1):
  for state in (0,1):
   for early in (0,1):
    f=original
    if cfg:
     for name in ('taps','sections','delay','slice_level','bit_hi','bit_lo'):
      marker='\tint '+name+' =';assert f.count(marker)==1;f=f.replace(marker,'\tshort '+name+' =')
    if state:
     for name in ('idx','nbits'):
      marker='\tint '+name+' =';assert f.count(marker)==1;f=f.replace(marker,'\tshort '+name+' =')
     assert f.count('\tint i;')==1;f=f.replace('\tint i;','\tshort i;')
    if early:
     typ='short' if state else 'int';marker='\t'+typ+' nbits = 0;\n';assert f.count(marker)==1;f=f.replace(marker,'');f=f.replace('\tconst short *fir =',marker+'\tconst short *fir =',1)
    label='baseline' if not(cfg or state or early) else 'cfg-%d-state-%d-early-%d'%(cfg,state,early)
    cells[label]=source[:a]+f+source[z:]
 assert len(set(cells.values()))==8
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-fsd-word';d.SOURCE_PATHS=('src/dsp/fpm_fsd.c',);d.variants=variants;d.main()
