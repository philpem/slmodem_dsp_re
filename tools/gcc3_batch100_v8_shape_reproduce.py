#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,original=d.function(source,'v8_txinit');cells={}
 for cursor in (0,1):
  for word in (0,1):
   f=original
   if word:
    assert f.count('\tint i;')==1;f=f.replace('\tint i;','\tshort i;')
   if cursor:
    marker='\tshort i;' if word else '\tint i;';f=f.replace(marker,marker+'\n\tshort *shape = v->tx_shape;')
    assert f.count('\t\tv->tx_shape[i] = 0;')==1;f=f.replace('\t\tv->tx_shape[i] = 0;','\t\t*shape++ = 0;')
   name='baseline' if not(cursor or word) else 'cursor-%d-word-%d'%(cursor,word)
   cells[name]=source[:a]+f+source[z:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-v8-shape';d.SOURCE_PATHS=('src/v8/V8global.c',);d.variants=variants;d.main()
