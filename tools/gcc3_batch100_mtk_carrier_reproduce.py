#!/usr/bin/env python3
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_mtk_phasor_reproduce as prior
original_compile=d.tc.compile_shell
def profiled_compile(compiler,flags,out,source):
 if '/inline-' in out:flags=list(flags)+['-D__FAST_MATH__']
 return original_compile(compiler,flags,out,source)
def variants(path,source):
 s=prior.variants(path,source)['word-1-predicate-1-member-1'];a,z,fn=d.function(s,'MTK_phasor');cells={'baseline':source}
 for phase,advance in itertools.product([False,True],repeat=2):
  f=fn
  if phase:f=f.replace('float x;','double x;')
  if advance:f=f.replace('float s;','double s;')
  cells['inline-phase-%d-advance-%d'%(phase,advance)]=s[:a]+f+s[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-mtk-carrier';d.SOURCE_PATHS=('src/service/PHASOR.c',);d.variants=variants;d.tc.compile_shell=profiled_compile;d.main()
