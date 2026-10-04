#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_agc_reproduce as agc

def variants(path,source):
 predecessor=agc.variants(path,source)['cursor-0-word-0-gate-1'];a,z,f=d.function(predecessor,'FPM_AGC_agc')
 for old,new in [('(short)level < (short)acquire_level && (short)mult == 0','((short)level < (short)acquire_level) & ((short)mult == 0)'),('(short)level < (short)squelch_level && (short)mult != 0','((short)level < (short)squelch_level) & ((short)mult != 0)')]:
  assert f.count(old)==1;f=f.replace(old,new)
 return {'baseline':source,'gate-word':predecessor,'gate-word-eager-inner':predecessor[:a]+f+predecessor[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-agc-boolean';d.SOURCE_PATHS=('src/dsp/fpm_agc.c',);d.variants=variants;d.main()
