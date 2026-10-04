#!/usr/bin/env python3
"""CID input cursor crossed with eager comparison combination."""
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'CID_MTD_detect');q=fn.replace('samples[i]','*samples++')
 cells={'baseline':source,'cursor-only':source[:a]+q+source[z:]}
 old='return (short)!(wide > 150 && wide / 2 > narrow);'
 assert old in q
 for typ in ['int','short']:
  f=q.replace(old,'{\n\t\t'+typ+' above_floor = wide > 150;\n\t\t'+typ+' above_residual = wide / 2 > narrow;\n\t\treturn !(above_floor & above_residual);\n\t}')
  cells['predicates-'+typ]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-cid-predicates';d.SOURCE_PATHS=('src/service/Cidmtd.c',);d.variants=variants;d.main()
