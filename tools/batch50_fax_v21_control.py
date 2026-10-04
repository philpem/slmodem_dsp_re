#!/usr/bin/env python3
"""V21 transmit late child and original request flag snapshot controls."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/V21t_stc.c',);d.OUT_NAME='batch50-fax-v21-control'
def variants(path,source):
 cells={'baseline':source};a,z,fn=d.function(source,'V21TX_control')
 for owner,flags,label in [(True,False,'late-child'),(False,True,'flags-snapshot'),(True,True,'late-snapshot')]:
  f=fn
  if owner:
   old='\tstruct v21_tx_hdx *hdx = tx->hdx;';assert old in f;f=f.replace(old,'\tstruct v21_tx_hdx *hdx;')
   marker='\thdx->int_0004 =';assert marker in f;f=f.replace(marker,'\thdx = tx->hdx;\n'+marker)
  if flags:
   f=f.replace('\n{\n','\n{\n\tunsigned char flags;\n',1).replace('arg->flags','flags')
   marker='\thdx->int_0004 =';f=f.replace(marker,'\tflags = arg->flags;\n'+marker)
  cells[label]=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
