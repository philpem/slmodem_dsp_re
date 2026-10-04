#!/usr/bin/env python3
"""V21 original flags snapshot and TEST/SETNE boolean lowering."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V21r_stc.c',);d.OUT_NAME='batch100-fax-v21-control'
def variants(path,s):
 a,z,f=d.function(s,'V21RX_control');cells={}
 for saved,branch in product((False,True),repeat=2):
  x=f
  if saved:
   x=x.replace('struct v21rx_cfg *cfg = &rx->cfg;', 'struct v21rx_cfg *cfg = &rx->cfg;\n\tunsigned char flags;').replace('cfg->int_0008 = arg->int_0004;', 'cfg->int_0008 = arg->int_0004;\n\tflags = arg->flags;').replace('(arg->flags &','(flags &')
  if branch:
   arg='flags'if saved else'arg->flags'
   old='rx->hdx->int_0000 =\n\t\t('+arg+' & V21RXCTL_SET_HDX_INT0000) != 0;'
   new='if ('+arg+' & V21RXCTL_SET_HDX_INT0000)\n\t\trx->hdx->int_0000 = 1;\n\telse\n\t\trx->hdx->int_0000 = 0;'
   assert old in x;x=x.replace(old,new)
  l='-'.join(n for n,v in [('saved-flags',saved),('explicit-boolean-arm',branch)]if v)or'baseline';cells[l]=s[:a]+x+s[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
