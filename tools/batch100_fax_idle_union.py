#!/usr/bin/env python3
"""Byte flag update followed by original union word carrier guard."""
import playbook_small_patterns as d
from batch100_fax_rx_epoch import variants as epoch_variants
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V17r_prc.c','src/fax/V27r_prc.c');d.OUT_NAME='batch100-fax-idle-union'
def variants(path,s):
 n=path.split('/')[-1][1:3];fn='RxHdxIdleV'+n;a,z,f=d.function(s,fn)
 if n=='17':f=f.replace('RXROOT(modem)->result.byte.flags & V17RX_FLAG_CARRIER','RXROOT(modem)->result.word & (V17RX_FLAG_CARRIER << 8)')
 else:f=f.replace('((struct v27_rx *)modem)->result.byte.flags & V27_STATUS_FLAG_CARRIER','((struct v27_rx *)modem)->result.word & (V27_STATUS_FLAG_CARRIER << 8)')
 # Include stable epoch winner, no experiments on those tokens here.
 label='carrier-positive-count-member'+('-short-epoch-result' if n=='17' else '')
 current=epoch_variants(path,s)[label];qa,qz,qf=d.function(current,'RxHdxEpochDetV'+n);oa,oz,_=d.function(s,'RxHdxEpochDetV'+n)
 changed=s[:a]+f+s[z:];oa,oz,_=d.function(changed,'RxHdxEpochDetV'+n);changed=changed[:oa]+qf+changed[oz:]
 return {'baseline':s,'word-carrier-after-byte-store':changed}
d.variants=variants
if __name__=='__main__':d.main()
