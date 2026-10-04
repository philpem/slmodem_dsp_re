#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
import gcc3_batch50_v90_jd_phase as parent
d.OUT_NAME='gcc3-batch50-v90-jd-shift'
def variants(path,source):
 cells={}
 for setter,ctor in itertools.product((False,True),repeat=2):
  flags='-'.join(n for n,v in [('setter-branch',setter),('ctor-branch',ctor)] if v) or 'baseline'
  text=parent.variants(path,source)[flags]
  if setter:text=text.replace('if (q & (1 << k))','if ((q >> k) & 1)')
  if ctor:text=text.replace('if (phase & (1 << u))','if ((phase >> u) & 1)')
  cells[flags]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
