#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/Sgd.c',);d.OUT_NAME='gcc3-batch100-fax-sgd-det-count'
def variants(path,source):
 cells={'baseline':source}
 for copy,corr,label in [(True,False,'copy-counts'),(False,True,'correlation-count'),(True,True,'both-count-classes')]:
  start,end,fn=d.function(source,'SGD_sequence_det')
  if copy:
   old='for (j = (unsigned short)((unsigned short)(s->hist_span - n) - 1);\n\t     j != 0xffffu; j = (unsigned short)(j - 1))';assert fn.count(old)==1
   fn=fn.replace(old,'j = (unsigned short)(s->hist_span - n);\n\twhile (j--)')
   old='for (j = (unsigned short)((unsigned short)n - 1);\n\t     j != 0xffffu; j = (unsigned short)(j - 1))';assert fn.count(old)==1
   fn=fn.replace(old,'j = (unsigned short)n;\n\twhile (j--)')
  if corr:
   old='for (c = (unsigned short)(s->cfg.det.ref_len - 1);\n\t\t     c != 0xffffu; c = (unsigned short)(c - 1))';assert fn.count(old)==1
   fn=fn.replace(old,'c = s->cfg.det.ref_len;\n\t\twhile (c--)')
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
