#!/usr/bin/env python3
"""Transfer proved post-decrement/source-pointer use boundaries to dot product."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 a,z,fn=driver.function(source,'FPM_circ_dotp2');cells={'baseline':source}
 for index,dest in itertools.product((False,True),repeat=2):
  if not(index or dest):continue
  body=fn
  if index:body=body.replace('i >= 0; i--','i >= 0;').replace('i > widx; i--','i > widx;').replace('hist[i]', 'hist[i--]')
  if dest:
   for opening in ['for (i = widx; i >= 0;'+('' if index else ' i--')+') {','for (i = (short)(taps - 1); i > widx;'+('' if index else ' i--')+') {']:
    assert body.count(opening)==1
    body=body.replace(opening,opening+'\n\t\tconst short *at = c;\n\t\tc += stride;')
   body=body.replace(' * *c)', ' * *at)').replace('\n\t\tc += stride;\n\t}', '\n\t}')
  cells[f'index-{int(index)}-coef-{int(dest)}']=source[:a]+body+source[z:]
 assert len(cells)==len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-dotp';driver.SOURCE_PATHS=('src/dsp/fpm_div32.c',);driver.variants=variants;driver.main()
