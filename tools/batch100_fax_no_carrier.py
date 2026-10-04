#!/usr/bin/env python3
"""Original no-carrier ring-array snapshots and owner boundaries."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=tuple('src/fax/V'+n+'t_int.c'for n in ('17','27','29'));d.OUT_NAME='batch100-fax-no-carrier'
def variants(path,s):
 n=path.split('/')[-1][1:3];a,z,f=d.function(s,'TxNoCarrierV'+n);cells={}
 for owner,width in product((False,True), (False,True) if n=='27' else (False,)):
  x=f
  if n=='17':
   if owner:x=x.replace('struct fpm_smc_ring *ring;', 'struct fpm_smc_ring *ring;\n\tshort *sym;').replace('widx = ring->widx;', 'sym = ring->sym;\n\twidx = ring->widx;').replace('ring->sym[widx]', 'sym[widx]')
   if width:x=x.replace('next = (short)(widx + 1);\n\t\t\twidx = (short)(next < len ? next : 0);','next = (short)(widx + 1);\n\t\t\twidx = (short)((short)next < len ? next : 0);')
  elif n=='27':
   if owner:x=x.replace('for (i = 0; i < count; i++) {\n\t\tstruct v27_tx_source *prm = ((struct v27_tx *)modem)->source;','if (count != 0) {\n\t\tstruct v27_tx_source *prm = ((struct v27_tx *)modem)->source;\n\t\tfor (i = 0; i < count; i++) {').replace('\n\t}\n\n\tr =', '\n\t\t}\n\t}\n\n\tr =')
   if width:x=x.replace('widx = (short)((widx + 1 < len) ? widx + 1 : 0);', 'short next = (short)(widx + 1);\n\t\twidx = (short)(next < len ? next : 0);')
  else:
   if owner:x=x.replace('struct fpm_smc_ring *ring;', 'struct fpm_smc_ring *ring;\n\tshort *ip, *qp;').replace('widx = ring->widx;', 'ip = ring->i;\n\tqp = ring->q;\n\twidx = ring->widx;').replace('ring->i[widx]', 'ip[widx]').replace('ring->q[widx]', 'qp[widx]')
   if width:x=x.replace('next = (short)(widx + 1);\n\t\twidx = (short)(next < len ? next : 0);', 'widx = (short)(widx + 1);\n\t\twidx = (short)(widx < len ? widx : 0);').replace('\t\tshort next;\n','')
  l='-'.join(nm for nm,v in [('original-owner',owner),('narrow-cursor-use',width)]if v)or'baseline';cells[l]=s[:a]+x+s[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
