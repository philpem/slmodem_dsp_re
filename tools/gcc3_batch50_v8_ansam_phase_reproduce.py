#!/usr/bin/env python3
"""Independent in-place envelope/carrier phase destination identities."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
from gcc3_batch50_v8_tone_reproduce import variants as previous,overlays as prior_overlays

def overlays(path,label):return {} if label=='baseline' else prior_overlays(path,'wide-1-cursor-1-short-1')
def variants(path,source):
 seed=previous(path,source)['wide-1-cursor-1-short-1'];cells={'baseline':source}
 for envelope,carrier in itertools.product((0,1),repeat=2):
  a,z,fn=d.function(seed,'v8_ansamgenerate')
  for name,active in [('envelope',envelope),('carrier',carrier)]:
   if not active:continue
   start=fn.index('\t\t'+name+' = ((');end=fn.index('\n',fn.index('t->'+name+'_phase = (short)'+name+';',start))
   fn=fn[:start]+'\t\tt->'+name+'_phase = (short)((t->'+name+'_phase\n\t\t\t\t\t  + t->'+name+'_step) & 0x3fff);\n\t\t'+name+' = (unsigned short)t->'+name+'_phase;'+fn[end:]
  cells[f'envelope-{envelope}-carrier-{carrier}']=seed[:a]+fn+seed[z:]
 assert len(set(cells.values()))==5
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v8-ansam-phase';d.SOURCE_PATHS=('src/v8/V8.c',);d.HEADER_OVERLAYS=overlays;d.variants=variants;d.main()
