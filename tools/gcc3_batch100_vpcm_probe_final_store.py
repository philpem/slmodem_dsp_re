#!/usr/bin/env python3
import gcc3_batch100_vpcm_probe_vector as p
import playbook_small_patterns as d
d.OUT_NAME='gcc3-batch100-vpcm-probe-final-store'
def variants(path,source):
 prior=p.variants(path,source);vector=prior['staged-probe-vector']
 old='probe_levels[i] = (float)((long double)decade * 10.0L + 60.0L);'
 assert vector.count(old)==1
 return {'baseline':source,'explicit-final-cast-vector':vector,'implicit-final-store-vector':vector.replace(old,'probe_levels[i] = (long double)decade * 10.0L + 60.0L;')}
d.variants=variants
if __name__=='__main__':d.main()
