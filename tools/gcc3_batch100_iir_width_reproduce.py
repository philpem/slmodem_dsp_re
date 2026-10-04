#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_iir_result_reproduce as prior
def variants(path,source):
 s=prior.variants(path,source)['output-cursor-1-count-1-ff-1'];a,z,fn=d.function(s,'FPM_iir_filt');cells={'baseline':source}
 for inp,out,node in itertools.product([False,True],repeat=3):
  f=fn
  if inp:f=f.replace('int acc_in = x;','short acc_in = x;')
  if out:f=f.replace('int output;','short output;')
  if node:f=f.replace('int w;','short w;')
  cells['input-%d-output-%d-node-%d'%(inp,out,node)]=s[:a]+f+s[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-iir-width';d.SOURCE_PATHS=('src/dsp/fpm_iir.c',);d.variants=variants;d.main()
