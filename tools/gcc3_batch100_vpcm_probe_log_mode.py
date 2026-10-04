#!/usr/bin/env python3
import gcc3_batch100_vpcm_probe_level_mode as p
import playbook_small_patterns as d
d.OUT_NAME='gcc3-batch100-vpcm-probe-log-mode'
def variants(path,source):
 vector=p.variants(path,source)['double-level-vector'];cells={'baseline':source,'long-double-inputs':vector}
 for scaled,log,label in [(1,0,'double-scaled'),(0,1,'double-log'),(1,1,'double-both')]:
  text=vector
  if scaled:
   old='(long double)probe[i]\n\t\t\t\t\t * 6.103515625e-05L';assert text.count(old)==1;text=text.replace(old,'probe[i]\n\t\t\t\t\t * 6.103515625e-05')
  if log:
   old='log10l((long double)probe_levels[i])';assert text.count(old)==1;text=text.replace(old,'log10((double)probe_levels[i])')
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
