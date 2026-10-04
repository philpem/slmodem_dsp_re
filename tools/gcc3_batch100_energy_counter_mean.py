#!/usr/bin/env python3
"""Post-sum loop endpoint is the original runtime mean denominator."""
from itertools import product
import playbook_small_patterns as d
from gcc3_batch100_energy_lifetime import variants as predecessors
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Beepgen.c',);d.OUT_NAME='gcc3-batch100-energy-counter-mean'
def variants(path,s):
 late=predecessors(path,s)['late-1-named-0'];cells={'baseline':s,'late-control':late}
 for counter,cursor in product((False,True),repeat=2):
  if not(counter or cursor):continue
  a,z,f=d.function(late,'bSearchEnergy')
  if counter:f=f.replace('sum / 1000','sum / i',1)
  if cursor:
   f=f.replace('unsigned int sum;','unsigned int sum;\n\tshort *window;',1)
   f=f.replace('\tsysdep_memcpy(buf2 + keep, new2, 2 * fresh);','\twindow = buf1 + 1000;\n\tsysdep_memcpy(buf2 + keep, new2, 2 * fresh);',1)
   f=f.replace('buf1[1000 + i]','window[i]')
  cells['counter-%d-window-%d'%(counter,cursor)]=late[:a]+f+late[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
