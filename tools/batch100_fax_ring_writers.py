#!/usr/bin/env python3
"""Original ring writers pre-store index update and postdecrement loops."""
from itertools import product
import playbook_small_patterns as d
from batch100_fax_frame_reverse import variants as frame_variants
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/faxvmififo.c',);d.OUT_NAME='batch100-fax-ring-writers'
def variants(path,source):
 a,z,fn=d.function(source,'faxvmi_write_fifo');cells={}
 # Preserve independently won frame body in every nonbaseline cell; no frame axes here.
 frame=frame_variants(path,source)['literal-unsigned-stride-outer-postdec'];fa,fz,ff=d.function(frame,'faxvmi_frame_reverse')
 sa,sz,sf=d.function(source,'faxvmi_frame_reverse')
 for post,early,late in product((False,True),repeat=3):
  x=fn
  if post:x=x.replace('for (i = count; i != 0; i--)','for (i = count; i-- != 0;)')
  if early:x=x.replace('fr->fifo[fr->wr] = *p++;\n\t\tfr->wr = (unsigned short)(fr->wr + 1);','fr->fifo[fr->wr++] = *p++;')
  if late:x=x.replace('struct faxvmi_framer *fr = vmi->framer;','struct faxvmi_framer *fr;').replace('for (i = count;', 'for (i = count;')
  if late:x=x.replace('\t\tif (fr->count >= fr->fifo_size)', '\t\tfr = vmi->framer;\n\t\tif (fr->count >= fr->fifo_size)')
  label='-'.join(n for n,v in [('postdec',post),('index-postfix',early),('late-framer',late)] if v) or 'baseline'
  cell=source[:a]+x+source[z:]
  if label!='baseline':cell=cell[:sa]+ff+cell[sz:]
  cells[label]=cell
 return cells
d.variants=variants
if __name__=='__main__':d.main()
