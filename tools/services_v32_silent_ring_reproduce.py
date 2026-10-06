#!/usr/bin/env python3
"""V32 silent transmit ring: countdown, mask update and real root ownership."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source}
 for count,mask,root in itertools.product((False,True),repeat=3):
  if not(count or mask or root):continue
  a,b,body=d.function(source,'TxNoCarrierV32')
  if count:body=body.replace('\tfor (i = 0; i < count; i++) {','\ti = count;\n\twhile (i--) {')
  if mask:
   old='\t\twidx = (short)(next < limit ? next : 0);';assert body.count(old)==1
   body=body.replace(old,'\t\twidx = next;\n\t\tif (widx >= limit)\n\t\t\twidx = 0;')
  if root:
   body=body.replace('\tstruct v32_symout *ring;\n\tstruct v32_smc *smc;\n','')
   body=body.replace('\tring = &fp->symout;\n\tsmc = &fp->tx_smc;\n','')
   body=body.replace('ring->','fp->symout.').replace('smc->','fp->tx_smc.')
  cells[f'countdown-{int(count)}-mask-{int(mask)}-root-{int(root)}']=source[:a]+body+source[b:]
 return cells
if __name__=='__main__':
 d.REV='e0052eec';d.SOURCE_PATHS=('src/pump/v32/V32int.c',);d.OUT_NAME='services-v32-silent-ring'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
