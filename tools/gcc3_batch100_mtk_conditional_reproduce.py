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
 for negative,terminal in itertools.product([False,True],repeat=2):
  f=fn
  if negative:f=f.replace('if (x < 0.0f)\n\t\tx = (float)(x + 6.28318530718);','x = x < 0.0f ? x + 6.28318530718 : x;')
  if terminal:f=f.replace('if (s >= 3.141592653589793)\n\t\tp->phase = s - 6.28318530718;\n\telse\n\t\tp->phase = s;', 'p->phase = s >= 3.141592653589793 ? s - 6.28318530718 : s;')
  cells['inline-negative-%d-terminal-%d'%(negative,terminal)]=s[:a]+f+s[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-mtk-conditional';d.SOURCE_PATHS=('src/service/PHASOR.c',);d.variants=variants;d.tc.compile_shell=profiled_compile;d.main()
