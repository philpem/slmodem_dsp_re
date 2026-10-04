#!/usr/bin/env python3
"""V17 original whole-template copy versus member initialization boundaries."""
from itertools import product
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/V17tx.c',);d.OUT_NAME='batch50-fax-tx-templates'
def variants(path,source):
 cells={}
 for fifo,sdm,smc in product((False,True),repeat=3):
  text=source
  if fifo:
   assert text.count('struct fifo_cfg fc;')==1
   text=text.replace('struct fifo_cfg fc;','struct fifo_cfg fc = FIFO_CFG;').replace('\t\tfc.word0 = FIFO_CFG.word0;\n','')
  if sdm:
   assert text.count('struct fpm_sdm_cfg dcfg;')==1
   text=text.replace('struct fpm_sdm_cfg dcfg;','struct fpm_sdm_cfg dcfg = SDM_CFG;')
  if smc:
   old='\t\tscfg[0] = SMCv17_CFG[0];\n\t\tscfg[1] = SMCv17_CFG[1];'
   assert text.count(old)==1
   text=text.replace(old,'\t\tmemcpy(scfg, SMCv17_CFG, sizeof scfg);')
  label='-'.join(n for n,v in [('fifo',fifo),('sdm',sdm),('smc',smc)] if v) or 'baseline';cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
