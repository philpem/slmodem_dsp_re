#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_fsd_mask_reproduce as masks

def variants(path,source):
 bases=masks.variants(path,source);selected={'retained':bases['baseline'],'owners':bases['cfg-1-state-1-early-1-clear-1']};cells={}
 for label,s in selected.items():
  for outputs in (0,1):
   for periods in (0,1):
    a,z,f=d.function(s,'FPM_FSD_demodulate')
    if outputs:
     typ='short' if label=='owners' else 'int';assert f.count('\t'+typ+' nbits = 0;')==1
     f=f.replace('\t'+typ+' nbits = 0;','\tunsigned short nbits = 0;').replace('if (nbits > state->cfg.max_bits + 1)','if ((short)nbits > state->cfg.max_bits + 1)')
    if periods:
     assert f.count('\t\tint bit_samples, k, d;')==1;f=f.replace('\t\tint bit_samples, k, d;','\t\tunsigned short bit_samples;\n\t\tint k, d;')
    name='baseline' if label=='retained' and not(outputs or periods) else label+'-outputs-%d-periods-%d'%(outputs,periods)
    cells[name]=s[:a]+f+source[z:] if label=='retained' else s[:a]+f+s[z:]
 assert len(set(cells.values()))==8
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-fsd-counter';d.SOURCE_PATHS=('src/dsp/fpm_fsd.c',);d.variants=variants;d.main()
