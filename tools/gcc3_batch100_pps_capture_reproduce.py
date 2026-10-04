#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,original=d.function(source,'FPM_PPS_filter');cells={}
 for capture in (0,1):
  for narrow in (0,1):
   f=original
   if capture:
    mappings=[('state->cfg.imap','imap'),('state->cfg.qmap','qmap'),('state->cfg.coeff_i','coeff_i'),('state->cfg.coeff_q','coeff_q'),('state->hist_i','hist_i'),('state->hist_q','hist_q'),('src->sym','sym'),('src->i','in_i'),('src->q','in_q')]
    head=''
    for expr,name in mappings:
     assert expr in f;f=f.replace(expr,name)
     typ='short *' if name.startswith('hist_') else 'const short *'
     head+='\t'+typ+name+' = '+expr+';\n'
    f=f.replace('state->cfg.scale','scale');head+='\tint scale = state->cfg.scale;\n'
    marker='\tshort phases = state->cfg.phases;';f=f.replace(marker,head+marker,1)
   if narrow:
    old='\t\t\tridx = (short)(ridx + 1 < len ? ridx + 1 : 0);\n\t\t\twidx = (short)(widx + 1 < taps ? widx + 1 : 0);'
    new='\t\t\tridx++;\n\t\t\tif (ridx >= len)\n\t\t\t\tridx = 0;\n\t\t\twidx++;\n\t\t\tif (widx >= taps)\n\t\t\t\twidx = 0;'
    assert f.count(old)==1;f=f.replace(old,new)
   label='baseline' if not(capture or narrow) else 'capture-%d-narrow-%d'%(capture,narrow)
   cells[label]=source[:a]+f+source[z:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-pps-capture';d.SOURCE_PATHS=('src/dsp/fpm_pps.c',);d.variants=variants;d.main()
