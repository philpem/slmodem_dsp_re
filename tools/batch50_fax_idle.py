#!/usr/bin/env python3
"""Object-supported idle arm/child lifetime and count local controls."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=tuple('src/fax/V'+str(n)+'t_prc.c' for n in (17,27,29));d.OUT_NAME='batch50-fax-idle'
def variants(path,source):
 cells={'baseline':source};n=int(path.split('/V')[1].split('t_')[0]);a,z,fn=d.function(source,'TxHdxIdleV'+str(n))
 if n==27:cells['unsigned-count']=source[:a]+fn.replace('\tshort taken;','\tunsigned short taken;')+source[z:]
 else:
  for owner,arms,label in [(True,False,'child-after-status'),(False,True,'nonempty-first'),(True,True,'child-nonempty')]:
   f=fn
   if owner:
    old='\tstruct fax_fifo *fifo =\n\t\t(struct fax_fifo *)prm->fifo;';assert old in f
    f=f.replace(old,'\tstruct fax_fifo *fifo;')
    old='result.byte.status = V'+str(n)+'TX_STATUS_IDLE;';assert old in f
    f=f.replace(old,old+'\n\tfifo = (struct fax_fifo *)prm->fifo;')
   if arms:
    old='\tif (fifo->count == 0) {';assert old in f
    f=f.replace(old,'\tif (fifo->count != 0) {\n\t\tTxNextStateV'+str(n)+'(modem);\n\t\treturn 0;\n\t}\n\t{')
    tail='\n\tTxNextStateV'+str(n)+'(modem);\n\treturn 0;\n}';assert tail in f
    f=f.replace(tail,'\n}')
   cells[label]=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
