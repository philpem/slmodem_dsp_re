#!/usr/bin/env python3
"""CID input cursor crossed with eager comparison combination."""
import sys
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'CID_MTD_detect');q=fn.replace('samples[i]','*samples++')
 old='return (short)!(wide > 150 && wide / 2 > narrow);'
 new='{\n\t\tint above_floor = wide > 150;\n\t\tint above_residual = wide / 2 > narrow;\n\t\treturn !(above_floor & above_residual);\n\t}'
 assert old in q
 q=q.replace(old,new)
 reversed=q.replace('int above_floor = wide > 150;\n\t\tint above_residual = wide / 2 > narrow;','int above_residual = wide / 2 > narrow;\n\t\tint above_floor = wide > 150;')
 return {'baseline':source,'floor-first':source[:a]+q+source[z:],'ratio-first':source[:a]+reversed+source[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-cid-order';d.SOURCE_PATHS=('src/service/Cidmtd.c',);d.variants=variants;d.main()
