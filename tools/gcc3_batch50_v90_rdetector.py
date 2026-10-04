#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/pump/v90/V90RDetector.cpp',);d.OUT_NAME='gcc3-batch50-v90-rdetector'
def variants(path,source):
 cells={}
 methods=[('detectR','found','bits','taken',6),('detectRNot','verdict','reg','used',6),('detectRf','hit','sr','seen',12),('detectRfNot','answer','shifted','count',12)]
 for direct,guard in itertools.product((False,True),repeat=2):
  text=source
  for method,result,shift,count,length in methods:
   start,end,fn=d.function(text,'V90RDetector::'+method)
   if direct:
    fn=fn.replace('\tunsigned int '+shift+' = (unsigned int)signBits * 2u;\n','')
    old='\tif (sample > 0)\n\t\t'+shift+' |= 1u;\n\tsignBits = (unsigned short)'+shift+';'
    new='\tsignBits <<= 1;\n\tif (sample > 0)\n\t\tsignBits |= 1;'
    assert old in fn;fn=fn.replace(old,new)
   if guard:
    old='\tif ('+count+' != '+str(length)+') {\n\t\tsampleCount = '+count+';\n\t\treturn 0;\n\t}\n'
    assert old in fn;fn=fn.replace(old,'\tif ('+count+' == '+str(length)+') {\n')
    tail='\treturn '+result+';';fn=fn.replace(tail,'\t} else {\n\t\tsampleCount = '+count+';\n\t}\n'+tail)
   text=text[:start]+fn+text[end:]
  label='-'.join(n for n,v in [('member-update',direct),('group-guard',guard)] if v) or 'baseline';cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
