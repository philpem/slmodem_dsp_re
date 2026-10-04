#!/usr/bin/env python3
"""Cross verdict read boundaries with observed HI outer-loop/tap-count carriers."""
import itertools
import playbook_small_patterns as d
from next20_dsp_tone_verdict import variants as seed

def variants(path,source):
 a,z,original=d.function(source,'FPM_TONE_detect');cells={}
 for late,word in itertools.product((False,True),repeat=2):
  text=seed(path,source)['late-verdict-fields' if late else 'baseline']
  a,z,fn=d.function(text,'FPM_TONE_detect')
  if word:
   assert fn.count('\tint taps = state->cfg.len;')==1 and fn.count('\tint i;')==1
   fn=fn.replace('\tint taps = state->cfg.len;','\tshort taps = state->cfg.len;').replace('\tint i;','\tshort i;')
   text=text[:a]+fn+text[z:]
  cells['baseline' if not(late or word) else 'late%d-word%d'%(late,word)]=text
 assert len(set(cells.values()))==4
 return cells

if __name__=='__main__':
 d.REV='8af3af53';d.SOURCE_PATHS=('src/dsp/fpm_tone.c',);d.OUT_NAME='next20-dsp-tone-width';d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
