#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_vtb_literal_reproduce as literal

def variants(path,source):
 predecessor=literal.variants(path,source)['owners-absolute-affine-minimum-1-literal-1'];cells={'baseline':source}
 for metric in ('retained','word','word-split'):
  for recapture in (0,1):
   a,z,f=d.function(predecessor,'VTB_decoder')
   if metric!='retained':
    marker='\tint best, m, d, sym, sh;';assert f.count(marker)==1;f=f.replace(marker,'\tint best, sym, sh;\n\tshort m, d;')
   if metric=='word-split':
    old='\t\tif (d < m) {\n\t\t\tbest = j;\n\t\t\tm = d;\n\t\t}'
    new='\t\tif (d < m)\n\t\t\tbest = j;\n\t\tm = d < m ? d : m;'
    assert f.count(old)==1;f=f.replace(old,new)
   if recapture:
    marker='\tfor (j = 0; (short)j < state->depth; j++) {';assert f.count(marker)==1;f=f.replace(marker,'\tslot = (short)(state->ring * 8);\n'+marker)
   cells[metric+'-recapture-'+str(recapture)]=predecessor[:a]+f+predecessor[z:]
 assert len(set(cells.values()))==7
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-vtb-traceback';d.SOURCE_PATHS=('src/dsp/fpm_vtb.c',);d.variants=variants;d.main()
