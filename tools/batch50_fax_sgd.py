#!/usr/bin/env python3
"""SGD original quotient narrowing and postdecrement source controls."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/Sgd.c',);d.OUT_NAME='batch50-fax-sgd'
def variants(path,source):
 cells={'baseline':source};a,z,fn=d.function(source,'SGD_correlate')
 for scale,post,label in [(True,False,'short-scale'),(False,True,'postdec'),(True,True,'short-postdec')]:
  f=fn
  if scale:f=f.replace('\tint scale =','\tshort scale =')
  if post:f=f.replace('for (i = (short)(n - 1); i != -1; i = (short)(i - 1))','for (i = n; i-- != 0;)')
  cells[label]=source[:a]+f+source[z:]
 a,z,fn=d.function(source,'SGD_pattern_det');old='for (k = (short)(n - 1); k != -1; k = (short)(k - 1))';assert old in fn
 cells['pattern-postdec']=source[:a]+fn.replace(old,'for (k = n; k-- != 0;)')+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
