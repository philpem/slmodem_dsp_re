#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_v8_stability_reproduce as stability

def variants(path,source):
 predecessor=stability.variants(path,source)['word-0-unguarded-1'];a,z,f=d.function(predecessor,'checkSignalStability')
 assert f.count('\tif (delta < 0)')==1;f=f.replace('\tif (delta < 0)','\tif ((short)delta < 0)')
 return {'baseline':source,'unguarded-int':predecessor,'unguarded-int-word-predicate':predecessor[:a]+f+predecessor[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-v8-stability-predicate';d.SOURCE_PATHS=('src/v8/V8global.c',);d.variants=variants;d.main()
