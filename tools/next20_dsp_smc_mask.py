#!/usr/bin/env python3
"""Captured Boolean ring mask x witnessed counted cursor x wide index carrier."""
import itertools
import playbook_small_patterns as d
from gcc3_batch100_fax_smc_traversal_wrap import variants as traversal_seeds

def variants(path,source):
 cells={}
 for mask,traversal,wide in itertools.product((False,True),repeat=3):
  text=traversal_seeds(path,source)['counted-cursor' if traversal else 'baseline']
  if mask:
   old='\treturn (short)((next < len) ? next : 0);'
   assert text.count(old)==1
   new='\tint fits = next < len;\n\tunsigned int result = (unsigned int)(int)next & (unsigned int)-fits;\n\n\treturn (short)result;'
   text=text.replace(old,new)
  if wide:
   old='static short\nsmc_ring_advance(short widx, short len)'
   assert text.count(old)==1
   text=text.replace(old,'static int\nsmc_ring_advance(int widx, short len)')
   if mask:
    text=text.replace('\treturn (short)result;','\treturn (int)result;')
   else:
    text=text.replace('\treturn (short)((next < len) ? next : 0);','\treturn (next < len) ? (int)next : 0;')
   for name in ('SMCv17_encoder_dif','SMCv17_encoder_abs','SMCv17_encoder_tcm'):
    a,z,fn=d.function(text,name)
    assert fn.count('\tshort widx = ring->widx;')==1
    fn=fn.replace('\tshort widx = ring->widx;','\tint widx = ring->widx;').replace('\tring->widx = widx;','\tring->widx = (short)widx;')
    text=text[:a]+fn+text[z:]
  label='baseline' if not(mask or traversal or wide) else 'mask%d-cursor%d-wide%d'%(mask,traversal,wide)
  cells[label]=text
 assert len(set(cells.values()))==8
 return cells

if __name__=='__main__':
 d.REV='8af3af53';d.SOURCE_PATHS=('src/fax/Smc.c',);d.OUT_NAME='next20-dsp-smc-mask';d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
