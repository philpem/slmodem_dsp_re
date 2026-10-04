#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'FPM_iir_filt_II');cells={'baseline':source}
 for outer,inner,sample in itertools.product([False,True],repeat=3):
  if not(outer or inner or sample):continue
  f=fn
  if outer:f=f.replace('int i;','short i;')
  if inner:f=f.replace('int j;','short j = sections;').replace('for (j = (short)(sections - 1); j != -1; j = (short)(j - 1))','while (j--)')
  if sample:f=f.replace('int acc = samples[i];','short acc = samples[i];')
  cells['outer-%d-inner-%d-sample-%d'%(outer,inner,sample)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-iir-form1';d.SOURCE_PATHS=('src/dsp/fpm_iir.c',);d.variants=variants;d.main()
