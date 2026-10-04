#!/usr/bin/env python3
"""Object's paired backward histories loop in V8 notch filter."""
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,fn=d.function(source,'notch_filter')
 old='d->acc_c[2] = d->acc_c[1];\n\td->acc_d[2] = d->acc_d[1];\n\td->acc_c[1] = d->acc_c[0];\n\td->acc_d[1] = d->acc_d[0];'
 assert fn.count(old)==1
 fn=fn.replace(old,'for (i = 0; i < 2; i++) {\n\t\td->acc_c[2 - i] = d->acc_c[1 - i];\n\t\td->acc_d[2 - i] = d->acc_d[1 - i];\n\t}')
 return {'baseline':source,'paired-history-loop':source[:a]+fn+source[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v8-notch';d.SOURCE_PATHS=('src/v8/V8Detector.c',);d.variants=variants;d.main()
