#!/usr/bin/env python3
"""Existing abs-narrow lever and original FIFO config/capacity/index boundaries."""
from itertools import product
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/V21r_int.c','src/fax/fifo.c');d.OUT_NAME='batch50-fax-abs-fifo'
def variants(path,source):
 cells={'baseline':source}
 if path.endswith('V21r_int.c'):
  a,z,fn=d.function(source,'GetSNRV21');old='\t\tmag[i] = (short)(v < 0 ? -v : v);';assert old in fn
  cells['abs-before-narrow']=source[:a]+fn.replace(old,'\t\tv = v < 0 ? -v : v;\n\t\tmag[i] = (short)v;')+source[z:]
 else:
  a,z,fn=d.function(source,'FIFO_create')
  for copy,capacity,index in product((False,True),repeat=3):
   if not(copy or capacity or index):continue
   f=fn
   if copy:
    start=f.index('\tshort word0, size;');end=f.index('\n\tif (f == NULL)',start)
    f=f[:start]+'''\tstruct fifo_cfg config;

\tif (cfg != NULL)
\t\tconfig = *cfg;
\telse
\t\tconfig = FIFO_CFG;
'''+f[end:]
    f=f.replace('(unsigned)(unsigned short)size * 2','(unsigned)(unsigned short)config.size * 2')
    f=f.replace('\tf->short_000 = word0;\n\tf->size = size;','\tmemcpy(f, &config, sizeof config);').replace('\tf->fill = fill;\n','')
   if capacity:f=f.replace('i < f->size','i < (unsigned short)f->size')
   if index:f=f.replace('f->buf[(unsigned short)i]','f->buf[i]')
   text=source[:a]+f+source[z:]
   if copy:text='#include <string.h>\n'+text
   label='-'.join(n for n,v in [('cfg-copy',copy),('unsigned-capacity',capacity),('signed-index',index)] if v);cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
