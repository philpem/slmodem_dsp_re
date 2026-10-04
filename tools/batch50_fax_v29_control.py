#!/usr/bin/env python3
"""Independent original V29 receive control request-byte snapshots."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/V29r_stc.c',);d.OUT_NAME='batch50-fax-v29-control'
def variants(path,source):
 cells={'baseline':source};a,z,fn=d.function(source,'V29RX_control')
 for first,last,label in [(True,False,'ctl1-snapshot'),(False,True,'ctl0-snapshot'),(True,True,'both-snapshots')]:
  f=fn
  decl='\tunsigned char '+(', '.join(x for enabled,x in [(first,'ctl1'),(last,'ctl0')] if enabled))+';\n'
  f=f.replace('\n{\n','\n{\n'+decl,1)
  if first:
   f=f.replace('req->ctl1','ctl1');marker='\t((struct v29_rx *)modem)->det->int_0008 ='
   assert marker in f;f=f.replace(marker,'\tctl1 = req->ctl1;\n'+marker)
  if last:
   f=f.replace('req->ctl0','ctl0');marker='\tif (ctl0 & V29RXCTL_CTL0_BIT3)';assert marker in f;f=f.replace(marker,'\tctl0 = req->ctl0;\n'+marker)
  cells[label]=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
