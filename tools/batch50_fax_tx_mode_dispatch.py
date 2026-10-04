#!/usr/bin/env python3
"""V17 original bitrate decision tree and exact encoder store ordering."""
from itertools import product
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/V17tx.c',);d.OUT_NAME='batch50-fax-tx-mode-dispatch'
def variants(path,source):
 a,z,fn=d.function(source,'V17TX_create');cells={}
 for dispatch,stores in product((False,True),repeat=2):
  f=fn
  if dispatch:
   start=f.index('\tif (TXROOT(modem)->cfg.bitrate == 9600) {');end=f.index('\n\n\t/* ---- the private block:',start)
   f=f[:start]+'''\tswitch (TXROOT(modem)->cfg.bitrate) {
\tcase 9600:
\t\tTXPRIV(modem)->mode = 1;
\t\tbreak;
\tcase 12000:
\t\tTXPRIV(modem)->mode = 2;
\t\tbreak;
\tcase 7200:
\t\tTXPRIV(modem)->mode = 0;
\t\tbreak;
\tcase 14400:
\t\tTXPRIV(modem)->mode = 3;
\t\tbreak;
\tdefault:
\t\tTXPRIV(modem)->mode = 3;
\t\tTXROOT(modem)->result.byte.flags |= V17TX_RESULT_B1_BIT1;
\t\tTXROOT(modem)->result.byte.status = V17TX_RESULT_BYTE_07;
\t\tbreak;
\t}'''+f[end:]
  if stores:
   old='\tTXBLOCK(modem)->encoders[0] = SMCv17_encoder_dif;\n\tTXBLOCK(modem)->encoders[1] = SMCv17_encoder_abs;';assert old in f
   f=f.replace(old,'\tTXBLOCK(modem)->encoders[1] = SMCv17_encoder_abs;\n\tTXBLOCK(modem)->encoders[0] = SMCv17_encoder_dif;')
  label='-'.join(n for n,v in [('mode-switch',dispatch),('encoder-stores',stores)] if v) or 'baseline';cells[label]=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
