#!/usr/bin/env python3
"""SDM compound feedback versus left-associated assignment."""
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'FPM_SDM_descrambler')
 old='*data = (unsigned short)\n\t\t\t((reg >> shift1) ^ in ^ (reg >> shift2));'
 assert old in fn
 q=fn.replace(old,'*data ^= (reg >> shift1) ^ (reg >> shift2);')
 q=q.replace('*data &= mask;\n\t\tdata++;','*data++ &= mask;')
 olddecl='const int shift1 = sdm->shift1;\n\tconst int shift2 = sdm->shift2;\n\tconst int mask = sdm->mask;\n\tconst int notmask = sdm->notmask;'
 newdecl='const int mask = sdm->mask;\n\tconst int notmask = sdm->notmask;\n\tconst int shift1 = sdm->shift1;\n\tconst int shift2 = sdm->shift2;'
 assert olddecl in q
 return {'baseline':source,'compound-post':source[:a]+q+source[z:],'config-field-order':source[:a]+q.replace(olddecl,newdecl)+source[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-sdm-config';d.SOURCE_PATHS=('src/dsp/fpm_sdm.c',);d.variants=variants;d.main()
