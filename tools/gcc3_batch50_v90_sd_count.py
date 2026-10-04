#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
import gcc3_batch50_v90_sd_boundaries as parent
d.OUT_NAME='gcc3-batch50-v90-sd-count'
def variants(path,source):
 seed=parent.variants(path,source)['endpoints-late-accumulators-common-verdict'];cells={'baseline':source}
 for count,branch in itertools.product((False,True),repeat=2):
  text=seed
  if count:
   text=text.replace('unsigned int i = historyLength - 1;','unsigned int i = historyLength;').replace('float *previous = history + historyLength - 2;','float *previous = history + i - 2;').replace('float *destination = history + historyLength - 1;','float *destination = history + i - 1;\n\ti--;')
  if branch:text=text.replace('if (run >= limit) result = 1;','if (run < limit) result = 0;\n\t\t\telse result = 1;')
  label='-'.join(n for n,v in [('count-after-endpoints',count),('explicit-low-else',branch)] if v) or 'seed'
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
