#!/usr/bin/env python3
"""Original V17 saved scrambler register callback ownership."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V17t_int.c',);d.OUT_NAME='batch100-fax-v17-scrambler-owner'
def variants(path,s):
 a,z,f=d.function(s,'SetTxModeV17');out={}
 for before,after in ((False,False),(True,False),(False,True),(True,True)):
  label='baseline'if not(before or after)else'-'.join(x for flag,x in((before,'before-whole-owner'),(after,'current-owner-delayed-restore'))if flag)
  x=f
  if before:
   x=x.replace('\tsdm = &fp->sdm;\n\tsaved_reg = sdm->reg;\n\tSDM_init(sdm, &sdmcfg);','\tsaved_reg = fp->sdm.reg;\n\tSDM_init(&fp->sdm, &sdmcfg);')
   if not after:x=x.replace('\tsdm->reg = saved_reg;','\tfp->sdm.reg = saved_reg;')
  if after:
   old='\tsdm->reg = saved_reg;';assert old in x;x=x.replace(old+'\n','')
   x=x.replace('\tfp->smc.r10 = 0;','\tfp->smc.r10 = 0;\n\tfp->sdm.reg = saved_reg;')
  if before:x=x.replace('\tstruct fpm_sdm *sdm;\n','')
  out[label]=s[:a]+x+s[z:]
 assert len(set(out.values()))==4
 return out
d.variants=variants
if __name__=='__main__':d.main()
