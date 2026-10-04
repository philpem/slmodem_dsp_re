#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_mtk_phasor_reproduce as prior
original_compile=d.tc.compile_shell
def profiled_compile(compiler,flags,out,source):
 if '/inline-' in out:flags=list(flags)+['-D__FAST_MATH__']
 return original_compile(compiler,flags,out,source)
def variants(path,source):
 full=prior.variants(path,source)['word-1-predicate-1-member-1'];cells={'baseline':source}
 for owner,s in [('original',source),('source',full)]:
  for library in ('float','double-float-mod','double-double-mod'):
   t=s
   if library!='float':t=t.replace('fmodf(p->phase, 6.28318530718f)','fmod(p->phase, 6.28318530718'+('f' if library=='double-float-mod' else '')+')')
   cells['inline-'+owner+'-'+library]=t
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-mtk-library';d.SOURCE_PATHS=('src/service/PHASOR.c',);d.variants=variants;d.tc.compile_shell=profiled_compile;d.main()
