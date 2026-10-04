#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V90AutoDigitalImpDetector.cpp',);d.OUT_NAME='gcc3-batch100-v90-adid-reset-width';d.DUMP_FLAGS=('-dr',)
def variants(path,source):
 start,end,fn=d.function(source,'V90AutoDigitalImpDetector::reset');cells={'baseline':source}
 for inner,outer,label in [(1,0,'unsigned-inner'),(0,1,'unsigned-phase-use'),(1,1,'both-width-uses')]:
  text=fn
  if inner:
   assert text.count('short ci;')==1;text=text.replace('short ci;','unsigned short ci;')
  if outer:
   old='phase < V90ADID_PHASES';assert text.count(old)==1;text=text.replace(old,'(unsigned short)phase < V90ADID_PHASES')
  cells[label]=source[:start]+text+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
