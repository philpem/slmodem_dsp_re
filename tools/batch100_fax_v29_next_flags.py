#!/usr/bin/env python3
"""Original V29 receive transition byte flag writes."""
import playbook_small_patterns as d
from batch100_fax_v29_rx_flags import byte_writes
from batch100_fax_rx_epoch import variants as epoch_variants
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V29r_prc.c',);d.OUT_NAME='batch100-fax-v29-next-flags'
def variants(path,s):
 x=epoch_variants(path,s)['carrier-positive-byte-flags'];a,z,f=d.function(x,'RxNextStateV29');new=byte_writes(f);assert f!=new
 return {'baseline':s,'byte-transition-flags':x[:a]+new+x[z:]}
d.variants=variants
if __name__=='__main__':d.main()
