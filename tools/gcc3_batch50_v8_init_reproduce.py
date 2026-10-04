#!/usr/bin/env python3
"""V8 init loop counter widths and receiver component ownership."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
from gcc3_batch50_v8_flip_reproduce import variants as previous

def variants(path,source):
 seed=previous(path,source)['promoted-input'];cells={'baseline':source}
 for rx,tx,owner in itertools.product((0,1),repeat=3):
  text=seed
  for name,short in [('v8_rxinit',rx),('v8_txinit',tx)]:
   a,z,fn=d.function(text,name)
   if short:assert fn.count('int i;')==1;fn=fn.replace('int i;','short i;',1)
   if owner and name=='v8_rxinit':fn=fn.replace('v->rx.','rx->').replace('{\n','{\n\tstruct v8_rx *rx = &v->rx;\n',1)
   text=text[:a]+fn+text[z:]
  cells[f'rx-short-{rx}-tx-short-{tx}-rx-owner-{owner}']=text
 assert len(set(cells.values()))==9
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v8-init';d.SOURCE_PATHS=('src/v8/V8global.c',);d.variants=variants;d.main()
