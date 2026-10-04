#!/usr/bin/env python3
"""Captured ring predicate crossed with witnessed owner/result boundaries."""
from itertools import product
import playbook_small_patterns as d
import batch100_fax_no_carrier as prior

def variants(path,s):
 n=path.split('/')[-1][1:3];cells={}
 for mask,owner,wide in product((False,True),repeat=3):
  label='baseline' if not(mask or owner or wide) else 'mask-%d-owner-%d-wide-%d'%(mask,owner,wide)
  parent='original-owner' if owner else 'baseline'
  text=prior.variants(path,s)[parent]
  a,z,f=d.function(text,'TxNoCarrierV'+n)
  if mask:
   if n=='27':
    old='widx = (short)((widx + 1 < len) ? widx + 1 : 0);'
    new='int next = (short)(widx + 1);\n\t\tint fits = (short)next < len;\n\t\twidx = (short)(next & -fits);'
   else:
    old='next = (short)(widx + 1);\n'+ ('\t\t\t' if n=='17' else '\t\t')+'widx = (short)(next < len ? next : 0);'
    tabs='\t\t\t' if n=='17' else '\t\t'
    f=f.replace(tabs+'short next;',tabs+'int next;\n'+tabs+'int fits;')
    new='next = (short)(widx + 1);\n'+tabs+'fits = (short)next < len;\n'+tabs+'widx = (short)(next & -fits);'
   assert f.count(old)==1,(n,old);f=f.replace(old,new)
  if wide:
   if n=='27':f=f.replace('short widx = ring->widx;', 'int widx = ring->widx;')
   else:f=f.replace('short widx, len;', 'int widx;\n\tshort len;')
   if mask:f=f.replace('widx = (short)(next & -fits);','widx = next & -fits;')
  cells[label]=text[:a]+f+text[z:]
 return cells

if __name__=='__main__':
 d.REV='8af3af53';d.SOURCE_PATHS=tuple('src/fax/V'+n+'t_int.c' for n in ('17','27','29'))
 d.OUT_NAME='next20-no-carrier-mask';d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
