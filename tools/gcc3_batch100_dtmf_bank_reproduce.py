#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,f=d.function(source,'DTMF_MTD_detect')
 begin=f.index('\tif (rx->rate == DTMF_RX_RATE_9600) {');end=f.index('\n\tfor (i = 0;',begin)
 old=f[begin:end];left,right=old.split('\t} else {\n',1)
 body1=left.split('\n',1)[1];body2=right.rsplit('\t}',1)[0]
 new='\tif (rx->rate != DTMF_RX_RATE_9600) {\n'+body2+'\t} else {\n'+body1+'\t}\n'
 f=f[:begin]+new+f[end:]
 return {'baseline':source,'8000-first':source[:a]+f+source[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-dtmf-bank';d.SOURCE_PATHS=('src/service/Dtmf_Detector.c',);d.variants=variants;d.main()
