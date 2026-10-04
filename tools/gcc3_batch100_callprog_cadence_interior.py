#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/callprog/Cadence.c',);d.OUT_NAME='gcc3-batch100-callprog-cadence-interior'
def variants(path,source):
 start,end,fn=d.function(source,'match_looped')
 old='\t\t\tif (adiff(off_last, c->off[last - k]) >= tol)';assert fn.count(old)==1
 new='\t\t\tif (adiff(off_last, c->off[last - k]) >= tol\n\t\t\t    || adiff(c->on[last], c->on[last - k]) >= tol)'
 fn=fn.replace(old,new)
 return {'baseline':source,'interior-on-comparison':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
