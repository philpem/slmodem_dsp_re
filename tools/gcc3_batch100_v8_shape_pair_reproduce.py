#!/usr/bin/env python3
import sys
from pathlib import Path
import playbook_small_patterns as d
import gcc3_batch100_v8_shape_reproduce as shape

def variants(path,source):
 predecessor=shape.variants(path,source)['cursor-1-word-1'];a,z,f=d.function(predecessor,'v8_txinit')
 old='\tv->tx_sym_a = v->tx_symbols;\n\tv->tx_sym_b = v->tx_symbols;';new='\tv->tx_sym_a = v->tx_sym_b = v->tx_symbols;'
 assert f.count(old)==1;f=f.replace(old,new)
 return {'baseline':source,'cursor-word':predecessor,'cursor-word-pair':predecessor[:a]+f+predecessor[z:]}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='856c1ecb';d.OUT_NAME='gcc3-batch100-v8-shape-pair';d.SOURCE_PATHS=('src/v8/V8global.c',);d.variants=variants;d.main()
