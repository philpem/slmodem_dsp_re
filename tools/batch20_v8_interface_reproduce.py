#!/usr/bin/env python3
"""Observable V8 GetMessage single-exit / signed member guard boundaries."""
import playbook_small_patterns as d
d.REV='93d7eee1';d.SOURCE_PATHS=('src/v8/V8Interface.c',);d.OUT_NAME='batch20-v8-interface'
def variants(path,source):
 a,b,fn=d.function(source,'V8GetMessage');cells={'baseline':source}
 for label,shared,direct in [('shared',True,False),('direct',False,True),('shared-direct',True,True)]:
  f=fn
  if direct:
   f=f.replace('\tint n = seq->wordidx;','\tint n;').replace('\tif (n <= 0)','\tif (seq->wordidx <= 0)').replace('\t/*\n\t * Too long','\tn = seq->wordidx;\n\n\t/*\n\t * Too long',1)
  if shared:
   f=f.replace('\tint rc = 0;','\tint rc = -1;')
   guard='seq->wordidx' if direct else 'n'
   f=f.replace('\tif ('+guard+' <= 0)\n\t\treturn V8_GET_EMPTY;','\tif ('+guard+' > 0) {\n\t\trc = 0;')
   f=f.replace('\treturn rc;','\t}\n\treturn rc;')
  cells[label]=source[:a]+f+source[b:]
 assert len(set(cells.values()))==4
 return cells
d.variants=variants
if __name__=='__main__':d.main()
