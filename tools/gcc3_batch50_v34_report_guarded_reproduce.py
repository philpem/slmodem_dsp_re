#!/usr/bin/env python3
"""V34 diagnostic's one signed entry guard and one scan latch."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_batch50_v34_report_loops_reproduce import variants as previous

def variants(path,source):
 seed=previous(path,source)['entry-0-bound-1'];a,z,fn=d.function(seed,'V34EchoReportCoeff')
 old='for (k = 0; k < n; k++)\n\t\tif (coeff[k] != 0)\n\t\t\tgoto coefficients;'
 assert fn.count(old)==1
 fn=fn.replace(old,'k = 0;\n\tif (n > 0) {\n\t\tdo {\n\t\t\tif (coeff[k] != 0)\n\t\t\t\tgoto coefficients;\n\t\t} while (++k < n);\n\t}')
 return {'baseline':source,'found-exclusive-seed':seed,'guarded-do-body':seed[:a]+fn+seed[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v34-report-guarded';d.SOURCE_PATHS=('src/pump/v34/v34filters.c',);d.variants=variants;d.main()
