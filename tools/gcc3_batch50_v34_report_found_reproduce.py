#!/usr/bin/env python3
"""V34 nonzero scan edge bypasses dead scan-counter comparison."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_batch50_v34_report_reproduce import variants as previous

def variants(path,source):
 seed=previous(path,source)['signed-1-capture-1-guards-1'];a,z,fn=d.function(seed,'V34EchoReportCoeff')
 assert fn.count('break;')==1;fn=fn.replace('break;','goto coefficients;')
 fn=fn.replace('\tif (k == n) {\n','\t{\n',1)
 marker='\n\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("?======= Coefficients'
 assert fn.count(marker)==1;fn=fn.replace(marker,'\ncoefficients:'+marker,1)
 return {'baseline':source,'source-seed':seed,'direct-found-edge':seed[:a]+fn+seed[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v34-report-found';d.SOURCE_PATHS=('src/pump/v34/v34filters.c',);d.variants=variants;d.main()
