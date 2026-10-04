#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V92CP.cpp',)
d.OUT_NAME='gcc3-batch50-v90-cp'
def variants(path,source):
 start,end,fn=d.function(source,'float2Bits');cells={}
 for switch,invert in itertools.product((False,True),repeat=2):
  body=fn
  if switch:
   body=body.replace('\tif (mode == 0) {','\tswitch (mode) {\n\tcase 0:').replace('\t} else if (mode == 1) {','\t\tbreak;\n\tcase 1:')
   assert body.endswith('\n\t}\n}');body=body[:-5]+'\n\t\tbreak;\n\t}\n}'
  if invert:
   for table in ('fltTable_2','fltTable_1'):
    old='\t\t\tif ('+table+'[i] > x) {\n\t\t\t\t*p = 0;\n\t\t\t} else {\n\t\t\t\t*p = 1;\n\t\t\t\tx -= '+table+'[i];\n\t\t\t}'
    new='\t\t\tif (!('+table+'[i] > x)) {\n\t\t\t\t*p = 1;\n\t\t\t\tx -= '+table+'[i];\n\t\t\t} else {\n\t\t\t\t*p = 0;\n\t\t\t}'
    assert body.count(old)==1;body=body.replace(old,new)
  label='-'.join(n for n,v in [('switch',switch),('subtraction-first',invert)] if v) or 'baseline';cells[label]=source[:start]+body+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
