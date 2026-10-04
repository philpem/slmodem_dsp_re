#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/Sgd.c',);d.OUT_NAME='gcc3-batch100-fax-sgd-control-width'
def variants(path,source):
 cells={'baseline':source}
 for local,label in [(True,'unsigned-receiving-local'),(False,'unsigned-use-cast')]:
  start,end,fn=d.function(source,'SGD_control')
  old='s->thresh = (short)(s->cfg.sym_bits * s->cfg.det.ref_len';assert fn.count(old)==1
  fn=fn.replace(old,'s->thresh = (short)('+('bits' if local else '(unsigned short)s->cfg.sym_bits')+' * s->cfg.det.ref_len')
  if local:
   anchor='\t\ts->pat_sr = 0;';assert fn.count(anchor)==1;fn=fn.replace(anchor,'\t\tunsigned short bits = s->cfg.sym_bits;\n\n'+anchor)
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
