#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,original=d.function(source,'FPM_atan');cells={}
 for intrinsic in (0,1):
  for axes in (0,1):
   for signed in (0,1):
    f=original
    if intrinsic:
     for v in ('x','y'):
      old='(unsigned short)(%s < 0 ? -%s : %s)'%(v,v,v)
      assert f.count(old)==1;f=f.replace(old,'(unsigned short)abs((int)%s)'%v)
    if axes:
     for v in ('x','y'):
      old='%s < 0 ? 0x4000 : 0'%v
      assert f.count(old)==1;f=f.replace(old,'((int)%s >> 31) & 0x4000'%v)
    if signed:
     assert f.count('(int)ratio :')==1;f=f.replace('(int)ratio :','(short)ratio :')
    text=source[:a]+f+source[z:]
    if intrinsic:text=text.replace('#include "dsplib/fpm.h"','#include "dsplib/fpm.h"\n#include <stdlib.h>')
    name='baseline' if not(intrinsic or axes or signed) else 'abs-%d-axes-%d-signed-%d'%(intrinsic,axes,signed)
    cells[name]=text
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-atan';d.SOURCE_PATHS=('src/dsp/fpm_atan.c',);d.variants=variants;d.main()
