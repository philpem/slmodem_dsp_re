#!/usr/bin/env python3
"""Original FIFO configuration load after transmitter initialization."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/cDATAtx.c',);d.OUT_NAME='batch100-fax-tx-config-lifetime'
def variants(path,s):
 a,z,f=d.function(s,'_tx_scrambled_ones_init');assert f.count('struct fifo_cfg local = FIFO_CFG;')==1;new=f.replace('struct fifo_cfg local = FIFO_CFG;','struct fifo_cfg local;');new=new.replace('\t_init_transmitter(ctx, rate_code);','\t_init_transmitter(ctx, rate_code);\n\tlocal = FIFO_CFG;');return{'baseline':s,'config-after-transmitter':s[:a]+new+s[z:]}
d.variants=variants
if __name__=='__main__':d.main()
