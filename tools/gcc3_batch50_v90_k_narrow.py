#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90ConstellationDesigner.cpp',)
d.OUT_NAME='gcc3-batch50-v90-k-narrow'
def variants(path,source):
 start,end,fn=d.function(source,'V90ConstellationDesigner::calcK');cells={'baseline':source}
 tail='\tl2 = (float)log10l((long double)2.0f);\n\treturn (float)log10l((long double)k) * (1.0f / l2);'
 assert fn.count(tail)==1
 for label,entry in [('wide-result-late-cast',False),('wide-entry-late-cast',True)]:
  body=fn.replace(tail,('\tlp = ' if entry else '\tlong double lp = ')+'log10l((long double)k);\n\tl2 = (float)log10l((long double)2.0f);\n\treturn (float)lp * (1.0f / l2);')
  if entry:body=body.replace('\tfloat l2;','\tlong double lp;\n\tfloat l2;')
  cells[label]=source[:start]+body+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
