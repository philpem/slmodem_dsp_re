#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/pump/v90/V92Jd.cpp',);d.OUT_NAME='gcc3-batch50-v90-jd-phase'
def variants(path,source):
 cells={}
 for setter,ctor in itertools.product((False,True),repeat=2):
  text=source
  for method,enabled,index,value in [('setJdPhase',setter,'k','q'),('V92Jd',ctor,'u','phase')]:
   if enabled:
    start,end,fn=d.function(text,'V92Jd::'+method)
    old='\tfor ('+index+' = 0; '+index+' <= 15; '+index+'++)\n\t\tphaseBits[V90JD_GROUP1 + 1 + '+index+'] =\n\t\t    (unsigned char)(('+value+' & (1 << '+index+')) != 0);'
    new='\tfor ('+index+' = 0; '+index+' <= 15; '+index+'++) {\n\t\tif ('+value+' & (1 << '+index+'))\n\t\t\tphaseBits[V90JD_GROUP1 + 1 + '+index+'] = 1;\n\t\telse\n\t\t\tphaseBits[V90JD_GROUP1 + 1 + '+index+'] = 0;\n\t}'
    assert old in fn;fn=fn.replace(old,new);text=text[:start]+fn+text[end:]
  label='-'.join(n for n,v in [('setter-branch',setter),('ctor-branch',ctor)] if v) or 'baseline';cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
