#!/usr/bin/env python3
"""Original byte reverser postdecrement and narrow-before-OR boundaries."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/faxvmi_utls.c',);d.OUT_NAME='batch100-fax-reverse-loops'
def reverse_fn(fn,outer,inner,shift):
 if outer:fn=fn.replace('for (i = count; i != 0; i--)','for (i = count; i-- != 0;)')
 if inner:fn=fn.replace('\t\tshort bit;','\t\tunsigned short bit;').replace('for (bit = 7; bit >= 0; bit--)','for (bit = 8; bit-- != 0;)')
 if shift:fn=fn.replace('out = (unsigned short)((out << 1) | (in & 1));','out = (unsigned short)(out << 1);\n\t\t\tout |= in & 1;')
 return fn

def variants(path,source):
 a,z,fn=d.function(source,'faxvmi_byte_reverse');cells={}
 for outer,inner,shift in product((False,True),repeat=3):
  label='-'.join(n for n,v in [('outer-postdec',outer),('inner-postdec',inner),('shift-narrow',shift)] if v) or 'baseline'
  cells[label]=source[:a]+reverse_fn(fn,outer,inner,shift)+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
