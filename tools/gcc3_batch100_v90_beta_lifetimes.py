#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V90Equalizer.cpp',);d.OUT_NAME='gcc3-batch100-v90-beta-lifetimes'
def variants(path,source):
 cells={'baseline':source}
 for whole,shifted,label in [(True,False,'whole-local'),(False,True,'shifted-before-publish'),(True,True,'both-lifetimes')]:
  text=source
  for name,field,scale in [('setLinearEquBeta','linearEquMmxShift','1.0e10f'),('setDfeBeta','dfeMmxShift','1.0e7f')]:
   start,end,fn=d.function(text,'V90Equalizer::'+name)
   if whole:
    old='long double scaled = (long double)beta * '+scale+';';assert fn.count(old)==1;fn=fn.replace(old,old+'\n\t\tint whole = (int)scaled;')
    assert fn.count('((int)scaled - scaled)')==1;fn=fn.replace('((int)scaled - scaled)','(whole - scaled)')
   if shifted:
    old='\t\t'+field+' = shift;';assert fn.count(old)==1;fn=fn.replace(old,'\t\tint shifted = one_shifted_by(shift);\n'+old)
    old='(long double)one_shifted_by(shift)';assert fn.count(old)==1;fn=fn.replace(old,'(long double)shifted')
   text=text[:start]+fn+text[end:]
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
