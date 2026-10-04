#!/usr/bin/env python3
"""FDSP initializer late inlined clear helpers and fresh channel owner."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'FDSP_Kernel_InitObj');cells={'baseline':source}
 loops='for (i = 0; i < FDSP_DLY; i++)\n\t\tb->dly[i] = 0.0f;\n\tfor (i = 0; i < 240; i++)\n\t\tb->coef[i] = 0.0f;\n\tfor (i = 0; i < FDSP_DLY; i++)\n\t\ta->dly[i] = 0.0f;\n\tfor (i = 0; i < 240; i++)\n\t\ta->coef[i] = 0.0f;'
 assert loops in fn
 for helper,fresh in itertools.product([False,True],repeat=2):
  if not(helper or fresh):continue
  f=fn
  if helper:f=f.replace(loops,'zFLTUTL_FloatMemSet(0.0f, b->dly, FDSP_DLY);\n\tzFLTUTL_FloatMemSet(0.0f, b->coef, 240);\n\tzFLTUTL_FloatMemSet(0.0f, a->dly, FDSP_DLY);\n\tzFLTUTL_FloatMemSet(0.0f, a->coef, 240);')
  if fresh:
   start=f.index('\n\tfor (i = 0; i < FDSP_DLY;' if not helper else '\n\tzFLTUTL_FloatMemSet(0.0f, b->dly,')
   f=f[:start]+'\n\t{\n\tstruct fdsp_channel *clear_a = k->chan_a;'+f[start:].replace('a->dly','clear_a->dly').replace('a->coef','clear_a->coef')
   f=f[:-1]+'\t}\n}'
  cells['helper-%d-fresh-%d'%(helper,fresh)]=source[:a]+f+source[z:]
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-fdsp-init';d.SOURCE_PATHS=('src/service/Fdspkrnl.c',);d.variants=variants;d.main()
