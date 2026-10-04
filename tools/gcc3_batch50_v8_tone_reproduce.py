#!/usr/bin/env python3
"""V8 cosine wide-formal x advancing output x ANSam short counter."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
from gcc3_batch50_v8_cosread_reproduce import variants as previous,overlays as prior_overlays

def overlays(path,label):
 if label=='baseline':return {}
 wide=int(label.split('-')[1]);return prior_overlays(path,'formal-signed-casts-1' if wide else 'baseline')
def variants(path,source):
 previous_cells=previous(path,source);cells={'baseline':source}
 for wide,cursor,narrow in itertools.product((0,1),repeat=3):
  text=previous_cells['formal-signed-casts-1' if wide else 'baseline']
  for name in ['v8_TONEq_generate','v8_ansamgenerate']:
   a,z,fn=d.function(text,name)
   if cursor:fn=fn.replace('i < V8_QUEUE_BLOCK; i++','i < V8_QUEUE_BLOCK; i++, out++').replace('out[i]','*out')
   if narrow and name=='v8_ansamgenerate':assert fn.count('int i;')==1;fn=fn.replace('int i;','short i;',1)
   text=text[:a]+fn+text[z:]
  cells[f'wide-{wide}-cursor-{cursor}-short-{narrow}']=text
 assert len(cells)==9
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v8-tone';d.SOURCE_PATHS=('src/v8/V8.c',);d.HEADER_OVERLAYS=overlays;d.variants=variants;d.main()
