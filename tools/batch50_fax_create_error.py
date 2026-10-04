#!/usr/bin/env python3
"""Original class1 constructor post-template NULL-owner failure exits."""
from itertools import product
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/class1.c',);d.OUT_NAME='batch50-fax-create-error'
def variants(path,source):
 a,z,fn=d.function(source,'fax_class1_create');cells={}
 for tx,rx in product((False,True),repeat=2):
  f=fn
  for child,enabled in [('c',tx),('a',rx)]:
   if not enabled:continue
   old='\t\tif (ctx->vmi_'+child+'_cfg != NULL)\n\t\t\tctx->vmi_'+child+' = FAXVMI_create(NULL, ctx->vmi_'+child+'_cfg);';assert old in f
   new='\t\tif (ctx->vmi_'+child+'_cfg == NULL) {\n\t\t\tif (dsplibs_debug_level > 1)\n\t\t\t\tdsplibs_debug_printf("Internal memory allocation error!\\n\\n");\n\t\t\treturn NULL;\n\t\t}\n\t\tctx->vmi_'+child+' = FAXVMI_create(NULL, ctx->vmi_'+child+'_cfg);'
   f=f.replace(old,new)
  label='-'.join(n for n,v in [('tx-error',tx),('rx-error',rx)] if v) or 'baseline';cells[label]=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
