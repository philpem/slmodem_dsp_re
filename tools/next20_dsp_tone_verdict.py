#!/usr/bin/env python3
"""Two-TU controls of actual tone verdict read boundaries."""
import playbook_small_patterns as d

def variants(path,source):
 a,z,fn=d.function(source,'FPM_TONE_detect')
 assert fn.count('\tint ratio = state->cfg.ratio;')==1 and fn.count('\tint min_level = state->cfg.min_level;')==1
 fn=fn.replace('\tint ratio = state->cfg.ratio;\n','').replace('\tint min_level = state->cfg.min_level;\n','')
 fn=fn.replace('(short)total < min_level','(short)total < state->cfg.min_level').replace('(ratio * (short)total)', '(state->cfg.ratio * (short)total)')
 return {'baseline':source,'late-verdict-fields':source[:a]+fn+source[z:]}

if __name__=='__main__':
 d.REV='8af3af53';d.SOURCE_PATHS=('src/dsp/fpm_tone.c',);d.OUT_NAME='next20-dsp-tone-verdict';d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
