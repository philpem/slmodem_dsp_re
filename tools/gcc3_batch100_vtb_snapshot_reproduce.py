#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,original=d.function(source,'VTB_decoder');cells={}
 for word in (0,1):
  for cursor in (0,1):
   for absolute in (0,1):
    f=original;prefix=source[:a]
    if word:
     assert f.count('\tint j, best, m, d, sym, sh;')==1
     f=f.replace('\tint j, best, m, d, sym, sh;','\tshort j;\n\tint best, m, d, sym, sh;')
    if cursor:
     f=f.replace('\tstruct vtb_path *node;','\tstruct vtb_path *node;\n\tstruct vtb_path *saved;')
     marker='\tfor (j = 0; (short)j <= 7; j++) {';assert f.count(marker)==1
     f=f.replace(marker,'\tsaved = node;\n'+marker)
     assert f.count('sym16[j] = node[j].sym;')==1;f=f.replace('sym16[j] = node[j].sym;','sym16[j] = saved++->sym;')
    if absolute:
     f=f.replace('\tint j, best, m, d, sym, sh;','\tint j, best, m, d, sym, sh;\n\tint ai, aq;') if not word else f.replace('\tint best, m, d, sym, sh;','\tint best, m, d, sym, sh;\n\tint ai, aq;')
     old='\tci = ((ri < 0 ? -ri : ri) + 0x400) >> 11;\n\tcq = ((rq < 0 ? -rq : rq) + 0x400) >> 11;'
     new='\tai = abs(ri);\n\taq = abs(rq);\n\tci = (ai + 0x400) >> 11;\n\tcq = (aq + 0x400) >> 11;'
     assert f.count(old)==1;f=f.replace(old,new);prefix=prefix.replace('#include "dsplib/vtb.h"','#include "dsplib/vtb.h"\n#include <stdlib.h>')
    label='baseline' if not(word or cursor or absolute) else 'word-%d-cursor-%d-abs-%d'%(word,cursor,absolute)
    cells[label]=prefix+f+source[z:]
 assert len(set(cells.values()))==8
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-vtb-snapshot';d.SOURCE_PATHS=('src/dsp/fpm_vtb.c',);d.variants=variants;d.main()
