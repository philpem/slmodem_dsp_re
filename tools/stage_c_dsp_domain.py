#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='5249ea38'
d.SOURCE_PATHS=('src/dsp/Notch.c','src/dsp/fpm_mtd.c')
d.OUT_NAME='stage-c-dsp'
d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
def variants(path,s):
 cells={'baseline':s}
 if path.endswith('Notch.c'):
  old='coef[1] * w + coef[0] * in + state[1]'
  assert s.count(old)==1
  cells['state-before-recursive']=s.replace(old,'coef[1] * w + (coef[0] * in + state[1])')
 else:
  for loop,width in [(True,False),(False,True),(True,True)]:
   t=s
   if loop:
    assert t.count('for (i = 0; i < count; i++)')==1
    t=t.replace('\tint i;\n','').replace('for (i = 0; i < count; i++)','while (count--)').replace('samples[i] >> 5','*samples++ >> 5')
   if width:
    t=t.replace('int wideband = state->wideband','short wideband = state->wideband').replace('\tint tone;','\tshort tone;').replace('\tint out_of_band;','\tshort out_of_band;')
   cells[('countdown' if loop else '')+('-short-energies' if width else '')]=t
 assert len(set(cells.values()))==len(cells)
 return cells
d.variants=variants
d.main()
