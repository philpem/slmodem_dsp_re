#!/usr/bin/env python3
"""Object-witnessed receive-control reload and clear/set controls."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=tuple('src/fax/V'+str(n)+'r_stc.c' for n in (17,21,27,29));d.OUT_NAME='batch50-fax-controls'
def variants(path,source):
 cells={'baseline':source}
 n=int(path.split('/V')[1].split('r_')[0]);start,end,fn=d.function(source,'V'+str(n)+'RX_control')
 if n==27:
  for request,owner,label in [(True,False,'request-reloads'),(False,True,'child-reloads'),(True,True,'both-reloads')]:
   f=fn
   if request:
    f=f.replace('\tunsigned char flags;\n','').replace('\tunsigned char mask;\n','').replace('\tflags = ctl->flags;\n','').replace('\tmask = ctl->mask;\n','').replace('if (flags &','if (ctl->flags &').replace('if (mask &','if (ctl->mask &')
   if owner:
    f=f.replace('\tstruct v27_rx_block *rxb;\n','').replace('\trxb = ((struct v27_rx *)rx)->rx;\n','').replace('rxb->','((struct v27_rx *)rx)->rx->')
   cells[label]=source[:start]+f+source[end:]
 else:
  lhs,cond={17:('RXCTL(modem)->r08','arg->flags_0d & V17RXCTL_SET_CTL_INT_0008'),21:('rx->hdx->int_0000','arg->flags & V21RXCTL_SET_HDX_INT0000'),29:('((struct v29_rx *)modem)->det->int_0008','req->ctl1 & V29RXCTL_CTL1_BIT4')}[n]
  old='\t'+lhs+' =\n\t\t('+cond+') != 0;'
  assert old in fn,(n,old)
  f=fn.replace(old,'\t'+lhs+' = 0;\n\tif ('+cond+')\n\t\t'+lhs+' = 1;')
  cells['clear-set']=source[:start]+f+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
