#!/usr/bin/env python3
"""Observable V8 DFT sample cursor x table indexing x post-phase sample read."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,fn=d.function(source,'v8_dftupdate');cells={}
 for cursor,table,late in itertools.product((0,1),repeat=3):
  body=fn
  if cursor:body=body.replace('j < nsamples; j++','j < nsamples; j++, samples++').replace('samples[j]','*samples')
  if table:
   body=body.replace('v8_cosread((unsigned char)idx)','v8_costbl[idx]').replace('v8_cosread((unsigned char)(idx + 0x40))','v8_costbl[(idx + 0x40) & 0xff]')
  if late:
   value='*samples' if cursor else 'samples[j]'
   body=body.replace('int x = '+value+';','int x;')
   body=body.replace('b->phase = (short)phase;','b->phase = (short)phase;\n\t\t\tx = '+value+';')
  label='baseline' if not(cursor or table or late) else f'cursor-{cursor}-table-{table}-late-{late}'
  cells[label]=source[:a]+body+source[z:]
 assert len(set(cells.values()))==8
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v8-dft-update';d.SOURCE_PATHS=('src/v8/V8Dftc.c',);d.variants=variants;d.main()
