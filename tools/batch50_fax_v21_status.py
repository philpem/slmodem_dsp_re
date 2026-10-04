#!/usr/bin/env python3
"""V21 status late alias-visible source flag sampling controls."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/V21t_stc.c',);d.OUT_NAME='batch50-fax-v21-status'
def variants(path,source):
 cells={'baseline':source};a,z,fn=d.function(source,'V21TX_status')
 load='\ttx_flags = ((unsigned char *)(void *)&tx->cfg)[V21TX_OBJ_FLAGS];\n';clear='\tst->flags1 &= (unsigned char)~V21_STATUS1_BIT0;'
 assert load in fn and clear in fn
 for before,masked,label in [(True,False,'late-before'),(False,False,'late-after'),(True,True,'masked-before'),(False,True,'masked-after')]:
  f=fn.replace(load,'');sample=load.rstrip('\n')
  if masked:sample+='\n\ttx_flags &= V21_STATUS_BIT2;';f=f.replace('st->flags = (unsigned char)(tx_flags & V21_STATUS_BIT2);','st->flags = tx_flags;')
  f=f.replace(clear,sample+'\n'+clear if before else clear+'\n'+sample)
  cells[label]=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
