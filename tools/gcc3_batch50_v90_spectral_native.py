#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/pump/v90/V90SpectralShapingFilter.cpp',);d.OUT_NAME='gcc3-batch50-v90-spectral-native'
def variants(path,source):
 cells={}
 for progress,metric in itertools.product((False,True),repeat=2):
  text=source
  for method,enabled in [('progress',progress),('getMetric',metric)]:
   if enabled:
    start,end,fn=d.function(text,'V90SpectralShapingFilter::'+method)
    fn=fn.replace('long double','float');text=text[:start]+fn+text[end:]
  label='-'.join(n for n,v in [('native-progress',progress),('native-metric',metric)] if v) or 'baseline';cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
