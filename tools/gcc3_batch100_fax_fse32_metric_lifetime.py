#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V17rxdec.c',);d.OUT_NAME='gcc3-batch100-fax-fse32-metric-lifetime'
def variants(path,source):
 cells={'baseline':source}
 for metric,distance,label in [(True,False,'complete-first-metric'),(False,True,'distance-sign-control'),(True,True,'complete-first-metric-distance-sign')]:
  start,end,fn=d.function(source,'FAX_FSE_decision_32pt')
  if metric:
   line='\t\tshort e3 = (short)(((d3q * d3q) >> 15) + ((d3i * d3i) >> 15));\n';assert fn.count(line)==1
   fn=fn.replace(line,'');anchor='\t\tshort d3q = (short)(DECv17_ANA_QMAP[3] - rq);\n';assert fn.count(anchor)==1;fn=fn.replace(anchor,anchor+line)
  if distance:
   old='rq >= DECv17_ANA_QMAP[k]';assert fn.count(old)==1;fn=fn.replace(old,'(rq - DECv17_ANA_QMAP[k]) >= 0')
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
