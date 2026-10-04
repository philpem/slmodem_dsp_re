#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90ConstellationDesigner.cpp','src/pump/v90/V92EchoCanceller.cpp')
d.OUT_NAME='gcc3-batch50-v90-k-history'
def variants(path,source):
 cells={'baseline':source}
 if path.endswith('V90ConstellationDesigner.cpp'):
  start,end,fn=d.function(source,'V90ConstellationDesigner::calcK')
  tail='\tl2 = (float)log10l((long double)2.0f);\n\treturn (float)log10l((long double)k) * (1.0f / l2);'
  assert fn.count(tail)==1
  for label,entry in [('log-product-first',False),('log-product-entry-local',True)]:
   body=fn.replace(tail,('\tlp = ' if entry else '\tfloat lp = ')+'(float)log10l((long double)k);\n\tl2 = (float)log10l((long double)2.0f);\n\treturn lp * (1.0f / l2);')
   if entry:body=body.replace('\tfloat l2;','\tfloat lp, l2;')
   cells[label]=source[:start]+body+source[end:]
 else:
  start,end,fn=d.function(source,'V92EchoCanceller::resetEchoHistory')
  for label,entry in [('count-before-owner',True),('count-before-length',False)]:
   body=fn.replace('\tunsigned int n;\n','').replace('for (n = 0;','for (;')
   old='\tV92Parameters *blk = params;' if entry else '\techoLength = echoDelay'
   body=body.replace(old,'\tunsigned int n = 0;\n'+old,1)
   cells[label]=source[:start]+body+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
