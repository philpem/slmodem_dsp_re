#!/usr/bin/env python3
"""FDSP initializer late inlined clear helpers and fresh channel owner."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'FDSP_Kernel_InitObj');cells={'baseline':source}
 old='k->status = 2;\n\tk->ntaps_a = 80;\n\tk->ntaps_b = 40;\n\ta->mu = FDSP_CHAN_A_MU;\n\tb->mu = 0.0f;'
 new='a->mu = FDSP_CHAN_A_MU;\n\tb->mu = 0.0f;\n\tk->status = 2;\n\tk->ntaps_a = 80;\n\tk->ntaps_b = 40;'
 assert old in fn
 for step,helper in itertools.product([False,True],repeat=2):
  if not(step or helper):continue
  f=fn
  if step:f=f.replace(old,new)
  if helper:
   for owner in ['b','a']:
    for field,count in [('dly','FDSP_DLY'),('coef','240')]:
     q='for (i = 0; i < '+count+'; i++)\n\t\t'+owner+'->'+field+'[i] = 0.0f;'
     assert q in f
     f=f.replace(q,'zFLTUTL_FloatMemSet(0.0f, '+owner+'->'+field+', '+count+');')
  cells['step-%d-helper-%d'%(step,helper)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-fdsp-init-default';d.SOURCE_PATHS=('src/service/Fdspkrnl.c',);d.variants=variants;d.main()
