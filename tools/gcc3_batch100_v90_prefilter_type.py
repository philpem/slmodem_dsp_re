#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V90PreFilter.cpp',);d.OUT_NAME='gcc3-batch100-v90-prefilter-type'
def variants(path,source):
 start,end,fn=d.function(source,'V90PreFilter::selectFilter')
 for i in (1,2):
  old='\t\t\tedprintf("V90PreFilter: Force coeff type array '+str(i)+'\\r\\n");\n\t\t\ttype = '+str(i)+';'
  new='\t\t\ttype = '+str(i)+';\n\t\t\tedprintf("V90PreFilter: Force coeff type array '+str(i)+'\\r\\n");'
  assert fn.count(old)==1;fn=fn.replace(old,new)
 return {'baseline':source,'type12-before-diagnostic':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
