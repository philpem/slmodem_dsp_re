#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V90AutoDigitalImpDetector.cpp',);d.OUT_NAME='gcc3-batch100-v90-adid-alt-init-dr';d.DUMP_FLAGS=('-dr',)
def variants(path,source):
 start,end,fn=d.function(source,'V90AutoDigitalImpDetector::getAltVarThresh');cells={'baseline':source}
 for below,count,label in [(1,0,'late-below'),(0,1,'late-count'),(1,1,'late-both')]:
  text=fn;add=''
  if below:
   assert text.count('float below = 0.0f;')==1;text=text.replace('float below = 0.0f;','float below;');add+='\tbelow = 0.0f;\n'
  if count:
   assert text.count('short count = 0;')==1;text=text.replace('short count = 0;','short count;');add+='\tcount = 0;\n'
  mark='\tlim = sum * 0.16666667f;\n';assert text.count(mark)==1;text=text.replace(mark,mark+add)
  cells[label]=source[:start]+text+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
