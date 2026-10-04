#!/usr/bin/env python3
"""Observed post-call energy narrowing crossed with literal 16-bit sign mask."""
import itertools
import playbook_small_patterns as d
from next20_dsp_tone_width import variants as parent

def variants(path,source):
 cells={'baseline':source}
 seed=parent(path,source)['late1-word1']
 for energy,mask in itertools.product((False,True),repeat=2):
  a,z,fn=d.function(seed,'FPM_TONE_detect')
  if energy:
   old='energy = (short)(((int)sample * sample) >> 15);'
   assert fn.count(old)==1
   fn=fn.replace(old,'energy = ((int)sample * sample) >> 15;')
   old='\t\tin_band = (filtered * filtered) >> 15;'
   assert fn.count(old)==1
   fn=fn.replace(old,'\t\tenergy = (short)energy;\n'+old)
  if mask:
   old='\tif (out_of_band < 0)\n\t\tout_of_band = 0;'
   assert fn.count(old)==1
   fn=fn.replace(old,'\tout_of_band = (~out_of_band >> 15) & out_of_band;')
  cells['control' if not(energy or mask) else 'energy%d-mask%d'%(energy,mask)]=seed[:a]+fn+seed[z:]
 assert len(set(cells.values()))==5
 return cells

if __name__=='__main__':
 d.REV='8af3af53';d.SOURCE_PATHS=('src/dsp/fpm_tone.c',);d.OUT_NAME='next20-dsp-tone-terminal';d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
