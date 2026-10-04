#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_fsd_word_reproduce as widths

def variants(path,source):
 cells={}
 for label,s in widths.variants(path,source).items():
  for clear in (0,1):
   a,z,f=d.function(s,'FPM_FSD_demodulate')
   if clear:
    old='\t\tidx = ((short)(idx + 1) < taps) ? (short)(idx + 1) : 0;'
    new='\t\tidx = (short)(idx + 1);\n\t\tif (idx >= taps)\n\t\t\tidx = 0;'
    assert f.count(old)==1;f=f.replace(old,new)
   name='baseline' if label=='baseline' and not clear else label+'-clear-'+str(clear)
   cells[name]=s[:a]+f+s[z:]
 assert len(set(cells.values()))==16
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-fsd-mask';d.SOURCE_PATHS=('src/dsp/fpm_fsd.c',);d.variants=variants;d.main()
