#!/usr/bin/env python3
"""Original FDSP echo quiet reuse, peak owner and count signedness."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'EchoCanceler');f=fn
 f=f.replace('if (fabs(x) < peak)\n\t\t\tquiet++;','{\n\t\tint low = fabs(x) < peak;\n\t\tif (low)\n\t\t\tquiet++;').replace('mu != 0.0f && fabs(x) < peak','mu != 0.0f && low').replace('\n\t}\n\t*verdict','\n\t\t}\n\t}\n\t*verdict')
 f=f.replace('peak = FDSP_FABS(hist[pos]);','{\n\t\tconst float *h = hist + pos;\n\t\tpeak = FDSP_FABS(h[0]);').replace('FDSP_FABS(hist[pos + k])','FDSP_FABS(h[k])').replace('\n\t\tpeak = peak * 0.5f;','\n\t\t}\n\t\tpeak = peak * 0.5f;').replace('int quiet = 0;','unsigned int quiet = 0;')
 q=f.replace('if (low)\n\t\t\tquiet++;','quiet += low != 0;')
 assert q!=f
 return {'baseline':source,'seed':source[:a]+f+source[z:],'arithmetic-truth-count':source[:a]+q+source[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-fdsp-truth-count';d.SOURCE_PATHS=('src/service/Fdspkrnl.c',);d.variants=variants;d.main()
