#!/usr/bin/env python3
"""Floating TONE eager normalized allocation guard."""
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'TONE_create');old='if (allocated && t->fir_len > 0)';assert old in fn
 q=fn.replace(old,'if ((allocated != 0) & (t->fir_len > 0))')
 return {'baseline':source,'eager-normalized':source[:a]+q+source[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-tone-create';d.SOURCE_PATHS=('src/service/TONE.c',);d.variants=variants;d.main()
