#!/usr/bin/env python3
import gcc3_batch100_vpcm_probe_vector as p
import playbook_small_patterns as d
d.OUT_NAME='gcc3-batch100-vpcm-probe-level-mode'
def variants(path,source):
 vector=p.variants(path,source)['staged-probe-vector']
 old='probe_levels[i] = (float)((long double)decade * 10.0L + 60.0L);'
 assert vector.count(old)==1
 return {'baseline':source,'long-double-vector':vector,'float-level-vector':vector.replace(old,'probe_levels[i] = decade * 10.0f + 60.0f;'),'double-level-vector':vector.replace(old,'probe_levels[i] = decade * 10.0 + 60.0;')}
d.variants=variants
if __name__=='__main__':d.main()
