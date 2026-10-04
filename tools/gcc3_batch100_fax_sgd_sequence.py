#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/Sgd.c',);d.OUT_NAME='gcc3-batch100-fax-sgd-sequence'
def variants(path,source):
 cells={'baseline':source}
 for owner,count,label in [(True,False,'output-before-select'),(False,True,'postdecrement'),(True,True,'output-before-select-postdecrement')]:
  start,end,fn=d.function(source,'SGD_sequence_gen')
  if owner:
   old='\t\tunsigned short sym;';assert fn.count(old)==1;fn=fn.replace(old,old+'\n\t\tunsigned short *slot = out++;')
   old='*out++ = sym;';assert fn.count(old)==1;fn=fn.replace(old,'*slot = sym;')
  if count:
   old='\tshort k;\n';assert fn.count(old)==1;fn=fn.replace(old,'')
   old='for (k = (short)(n - 1); k != -1; k = (short)(k - 1)) {';assert fn.count(old)==1;fn=fn.replace(old,'while (n--) {')
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
