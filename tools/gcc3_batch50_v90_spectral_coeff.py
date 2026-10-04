#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90SpectralShapingFilter.cpp',)
d.OUT_NAME='gcc3-batch50-v90-spectral-coeff'
def variants(path,source):
 cells={}
 for narrow,guard in itertools.product((False,True),repeat=2):
  start,end,fn=d.function(source,'V90SpectralShapingFilter::progress')
  if narrow:fn=fn.replace('long double b0, b1, b2, b3;','float b0, b1, b2, b3;')
  if guard:fn=fn.replace('if (left == 0)','if (left <= 0)')
  label='-'.join(n for n,v in [('float-coeff',narrow),('inclusive-zero',guard)] if v) or 'baseline'
  cells[label]=source[:start]+fn+source[end:]
 for stem,text in [('metric-only',source),('both-inclusive',cells['float-coeff-inclusive-zero'])]:
  start,end,fn=d.function(text,'V90SpectralShapingFilter::getMetric')
  fn=fn.replace('long double a0, a1, a2, a3;','float a0, a1, a2, a3;')
  cells[stem]=text[:start]+fn+text[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
