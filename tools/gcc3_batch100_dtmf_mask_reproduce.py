#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,original=d.function(source,'dtmf_test');cells={'baseline':source}
 start=original.index('\tok = (elo *');end=original.index('\n\n\tif (rest *',start)
 conditions=['elo * 7.94f >= ehi','ehi * 2.82f >= elo','max_lo * 0.96f >= elo','max_hi * 0.96f >= ehi','rest * 1.36f >= pair']
 for form in ('sequential','clear'):
  for early in (0,1):
   if form=='sequential':
    body='\tok = ('+conditions[0]+');\n'+''.join('\tok = ok && ('+c+');\n' for c in conditions[1:])
   else:
    body='\tok = 1;\n'+''.join('\tif (!('+c+'))\n\t\tok = 0;\n' for c in conditions)
   f=original[:start]+body.rstrip()+original[end:]
   if early:
    for stmt in ('max_lo = 0.0f;','max_hi = 0.0f;','sum = 0.0f;'):
     assert f.count('\t'+stmt)==1;f=f.replace('\t'+stmt+'\n','')
    marker='\tthreshold = (mode == DTMF_MODE_EUR) ? 0.0022f : 0.002f;'
    f=f.replace(marker,'\tmax_lo = 0.0f;\n\tmax_hi = 0.0f;\n\tsum = 0.0f;\n\n'+marker)
   cells[form+'-early-'+str(early)]=source[:a]+f+source[z:]
 assert len(set(cells.values()))==5
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-dtmf-mask';d.SOURCE_PATHS=('src/service/Dtmf.c',);d.variants=variants;d.main()
