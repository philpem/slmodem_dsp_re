#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_vtb_extended_reproduce as scratch
import gcc3_batch100_vtb_snapshot_reproduce as snapshot

def variants(path,source):
 foundation=scratch.variants(path,source)['extended-add-counter-1-metric-1']
 main=snapshot.variants(path,foundation)
 bases={'retained':main['baseline'],'owners':main['word-1-cursor-1-abs-1']}
 cells={'baseline':source}
 for name,s in bases.items():
  for minimum in (0,1):
   for cursor in (0,1):
    t=s
    if minimum:
     a,z,f=d.function(t,'vtb_acs')
     old='\t\tif (d < m) {\n\t\t\tbest = p0 + k;\n\t\t\tm = d;\n\t\t}'
     new='\t\tif (d < m)\n\t\t\tbest = p0 + k;\n\t\tm = d < m ? d : m;'
     assert f.count(old)==1;f=f.replace(old,new);t=t[:a]+f+t[z:]
    if cursor:
     a,z,f=d.function(t,'vtb_branch');assert f.count('p = bp[k];')==1;f=f.replace('p = bp[k];','p = *bp++;');t=t[:a]+f+t[z:]
    cells[name+'-minimum-%d-cursor-%d'%(minimum,cursor)]=t
 assert len(set(cells.values()))==9
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-vtb-min';d.SOURCE_PATHS=('src/dsp/fpm_vtb.c',);d.variants=variants;d.main()
