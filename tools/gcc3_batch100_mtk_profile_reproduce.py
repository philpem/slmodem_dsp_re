#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_mtk_phasor_reproduce as prior
original_compile=d.tc.compile_shell
def profiled_compile(compiler,flags,out,source):
 if '/fastmath-' in out:flags=list(flags)+['-ffast-math']
 return original_compile(compiler,flags,out,source)
def variants(path,source):
 s=prior.variants(path,source)['word-1-predicate-1-member-1']
 return {'baseline':source,'source-only':s,'fastmath-original':source,'fastmath-source':s}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-mtk-profile';d.SOURCE_PATHS=('src/service/PHASOR.c',);d.variants=variants;d.tc.compile_shell=profiled_compile;d.main()
