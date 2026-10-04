#!/usr/bin/env python3
import gcc3_batch100_vpcm_probe_vector as p
import playbook_small_patterns as d
d.OUT_NAME='gcc3-batch100-vpcm-probe-shared-result'
def variants(path,source):
 vector=p.variants(path,source)['staged-probe-vector'];cells={'baseline':source,'vector-readback':vector}
 old='\t\t\tprobe_levels[i] = (float)((long double)decade * 10.0L + 60.0L);'
 for typ,expr,label in [('long double','(long double)decade * 10.0L + 60.0L','shared-long-double'),('float','decade * 10.0f + 60.0f','shared-float')]:
  text=vector.replace('\t\t\tfloat decade;','\t\t\tfloat decade;\n\t\t\t'+typ+' level;')
  assert text.count(old)==1;text=text.replace(old,'\t\t\tlevel = '+expr+';\n\t\t\tprobe_levels[i] = level;').replace('L2[j++] = probe_levels[i];','L2[j++] = level;')
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
