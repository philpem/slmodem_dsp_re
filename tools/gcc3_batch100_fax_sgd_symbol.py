#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/Sgd.c',);d.OUT_NAME='gcc3-batch100-fax-sgd-symbol'
def variants(path,source):
 cells={'baseline':source}
 for width,count,label in [(True,False,'unsigned-bits'),(False,True,'postdecrement'),(True,True,'unsigned-bits-postdecrement')]:
  start,end,fn=d.function(source,'SGD_symbol_gen')
  if width:
   old='int bits = s->cfg.sym_bits;';assert fn.count(old)==1;fn=fn.replace(old,'unsigned short bits = (unsigned short)s->cfg.sym_bits;')
  if count:
   old='\tshort i;\n';assert fn.count(old)==1;fn=fn.replace(old,'')
   old='for (i = (short)(n - 1); i != -1; i = (short)(i - 1)) {';assert fn.count(old)==1;fn=fn.replace(old,'while (n--) {')
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
