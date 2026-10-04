#!/usr/bin/env python3
import playbook_small_patterns as d
import gcc3_batch100_v90_std_fraction as parent
d.OUT_NAME='gcc3-batch100-v90-std-scale'
def variants(path,source):
 seed=parent.variants(path,source)['argument-fraction-early-verdict'];cells={'baseline':source,'argument-early-seed':seed}
 for label,s in list(cells.items()):
  start,end,fn=d.function(s,'V90ConnectionEvaluator::evaluateMeanErrorStdPhase3')
  assert fn.count('10000.0f')==1;fn=fn.replace('10000.0f','10000.0')
  cells['double-scale'+('-argument-early' if label!='baseline' else '')]=s[:start]+fn+s[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
