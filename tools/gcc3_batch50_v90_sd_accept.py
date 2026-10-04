#!/usr/bin/env python3
import playbook_small_patterns as d
import gcc3_batch50_v90_sd_count as parent
d.OUT_NAME='gcc3-batch50-v90-sd-accept'
def variants(path,source):
 seed=parent.variants(path,source)['count-after-endpoints']
 winner=seed.replace('if (run >= limit) result = 1;','if (run < limit) return result;\n\t\t\tresult = 1;')
 return {'baseline':source,'seed':seed,'below-limit-live-return':winner}
d.variants=variants
if __name__=='__main__':d.main()
