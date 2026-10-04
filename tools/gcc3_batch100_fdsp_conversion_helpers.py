#!/usr/bin/env python3
"""Original two conversion-loop ownership through existing helper calls."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Beepgen.c',);d.OUT_NAME='gcc3-batch100-fdsp-conversion-helpers'
def variants(path,s):
 a,z,f=d.function(s,'FDSP_DP_Run');cells={}
 for incoming,outgoing in product((False,True),repeat=2):
  label='in-%d-out-%d'%(incoming,outgoing) if incoming or outgoing else 'baseline';g=f
  if incoming:g=g.replace('\tfor (i = 0; i < n; i++)\n\t\trx_flt[i] = (float)rx_lin[i] * (1.0f / 32000.0f);','\tzFLTUTL_Linear2Float(rx_lin, rx_flt, n, 1.0f / 32000.0f);',1)
  if outgoing:g=g.replace('\tfor (i = 0; i < n; i++)\n\t\ttx_lin[i] = (short)(tx_flt[i] * 32000.0f);','\tzFLTUTL_Float2Linear(tx_flt, tx_lin, n, 32000.0f);',1)
  cells[label]=s[:a]+g+s[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
