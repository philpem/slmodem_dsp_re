#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_atan_reproduce as atan

def shared(source):
 a,z,f=d.function(source,'FPM_atan')
 assert f.count('\tint a;')==1;f=f.replace('\tint a;','\tint a;\n\tint result;')
 assert f.count('*angle =')==6;f=f.replace('*angle =','result =')
 assert f.count('\t\treturn;')==2;f=f.replace('\t\treturn;','\t\tgoto done;')
 f=f[:-1]+'done:\n\t*angle = (short)result;\n}'
 return source[:a]+f+source[z:]
def variants(path,source):
 predecessor=atan.variants(path,source)['abs-1-axes-1-signed-1']
 return {'baseline':source,'integer-predecessor':predecessor,'retained-result':shared(source),'integer-result':shared(predecessor)}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-atan-result';d.SOURCE_PATHS=('src/dsp/fpm_atan.c',);d.variants=variants;d.main()
