#!/usr/bin/env python3
"""Original medium fax callback/member operand read boundaries."""
from itertools import product
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/cDATAtx.c','src/fax/faxvmi_hdlc.c');d.OUT_NAME='batch50-fax-medium-reads'
def variants(path,source):
 cells={'baseline':source}
 if path.endswith('cDATAtx.c'):
  a,z,fn=d.function(source,'_tx_scrambled_ones_state')
  for late,signed in product((False,True),repeat=2):
   if not(late or signed):continue
   f=fn
   if late:
    f=f.replace('unsigned short result = (unsigned short)*tx_count;','unsigned short result;').replace('\tstatus = FAXVMI_process','\tresult = (unsigned short)*tx_count;\n\tstatus = FAXVMI_process')
   if signed:
    old='ctx->tx_fifo->count >= (unsigned)ctx->tx_bytes_per_block';assert old in f
    f=f.replace(old,'(int)ctx->tx_fifo->count >= ctx->tx_bytes_per_block')
   label='-'.join(n for n,v in [('late-result',late),('signed-ready',signed)] if v);cells[label]=source[:a]+f+source[z:]
 else:
  a,z,fn=d.function(source,'faxvmi_hdlc_frame');old='\tstruct faxvmi_framer *fr;\n';assert old in fn
  f=fn.replace(old,'').replace('\tfr = vmi->framer;\n','').replace('fr->','vmi->framer->')
  cells['framer-reloads']=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
