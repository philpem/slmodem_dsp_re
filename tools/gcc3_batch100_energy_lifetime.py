#!/usr/bin/env python3
"""Original post-copy accumulators crossed with a named numeric window."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Beepgen.c',);d.OUT_NAME='gcc3-batch100-energy-lifetime'
def variants(path,s):
 a,z,f=d.function(s,'bSearchEnergy');cells={}
 for late,named in product((False,True),repeat=2):
  label='late-%d-named-%d'%(late,named) if late or named else 'baseline';g=f
  if late:
   g=g.replace('unsigned int sum = 0;','unsigned int sum;').replace('int mean, ret = 0;','int mean, ret;').replace('float acc = 0.0f;','float acc;')
   g=g.replace('\n\tfor (i = 0; i < 1000; i++)\n\t\tsum', '\n\tsum = 0;\n\tfor (i = 0; i < 1000; i++)\n\t\tsum',1)
   g=g.replace('\tmean = (int)(sum / 1000);','\tmean = (int)(sum / 1000);\n\tacc = 0.0f;',1)
   g=g.replace('\tif (buf2[1] != 0) {','\tret = 0;\n\tif (buf2[1] != 0) {',1)
  if named:
   g=g.replace('unsigned int keep = total - fresh, i;','unsigned int keep = total - fresh, i;\n\tunsigned int count;',1)
   marker='\n\tsum = 0;' if late else '\n\tfor (i = 0; i < 1000; i++)\n\t\tsum'
   g=g.replace(marker,'\n\tcount = 1000;'+marker,1)
   g=g.replace('i < 1000','i < count').replace('sum / 1000','sum / count')
  cells[label]=s[:a]+g+s[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
