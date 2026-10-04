#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V92CP.cpp',)
d.OUT_NAME='gcc3-batch50-v90-cp-owner'
def variants(path,source):
 start,end,fn=d.function(source,'float2Bits');cells={'baseline':source}
 for cached,argowner in itertools.product((False,True),repeat=2):
  body=fn.replace('\tif (mode == 0) {','\tswitch (mode) {\n\tcase 0:').replace('\t} else if (mode == 1) {','\t\tbreak;\n\tcase 1:')
  assert body.endswith('\n\t}\n}');body=body[:-5]+'\n\t\tbreak;\n\t}\n}'
  for table,limit in [('fltTable_2',15),('fltTable_1',6)]:
   if cached:
    old='for (i = 0; i <= '+str(limit)+'; i++) {'
    body=body.replace(old,old+'\n\t\t\tfloat weight = '+table+'[i];',1).replace(table+'[i] > x','weight > x').replace('x -= '+table+'[i];','x -= weight;')
   else:
    old='\t\t\t\t*p = 1;\n\t\t\t\tx -= '+table+'[i];'
    assert body.count(old)==1
    body=body.replace(old,'\t\t\t\tx -= '+table+'[i];\n\t\t\t\t*p = 1;')
  if argowner:
   body=body.replace('\tunsigned char *p;\n','').replace('p = &bits[15];','bits += 15;').replace('p = &bits[6];','bits += 6;').replace('*p =','*bits =').replace('p--;','bits--;')
  label=('cached-weight' if cached else 'subtract-before-byte')+('-argument-owner' if argowner else '')
  cells[label]=source[:start]+body+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
