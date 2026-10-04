#!/usr/bin/env python3
"""Original whole-block owner around SDM initializer callback."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V27t_int.c',);d.OUT_NAME='batch100-fax-v27-scrambler-owner'
def variants(path,s):
 a,z,f=d.function(s,'SetScramblerV27');out={}
 for before,after in ((False,False),(True,False),(False,True),(True,True)):
  label='baseline' if not(before or after)else '-'.join(x for flag,x in ((before,'before-whole-owner'),(after,'after-whole-owner'))if flag)
  x=f
  if before or after:x=x.replace('\tstruct sdmv27 *sdm;','\tstruct sdmv27 *sdm;\n\tstruct v27_tx_block *tx;')
  if before:
   x=x.replace('\tsdm = &((struct v27_tx *)modem)->tx->sdm;\n\treg = sdm->reg;','\ttx = ((struct v27_tx *)modem)->tx;\n\treg = tx->sdm.reg;')
   x=x.replace('SDMv27_init(sdm, &cfg);','SDMv27_init(&tx->sdm, &cfg);')
  if after:x=x.replace('\tsdm = &((struct v27_tx *)modem)->tx->sdm;\n\tsdm->reg = reg;','\ttx = ((struct v27_tx *)modem)->tx;\n\ttx->sdm.reg = reg;')
  if before and after:x=x.replace('\tstruct sdmv27 *sdm;\n','')
  out[label]=s[:a]+x+s[z:]
 assert len(set(out.values()))==4
 return out
d.variants=variants
if __name__=='__main__':d.main()
