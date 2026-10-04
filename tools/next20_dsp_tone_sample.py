#!/usr/bin/env python3
"""Direct ownership of the addressable tone sample consumed by in-place IIR."""
import playbook_small_patterns as d
from next20_dsp_tone_terminal import variants as parent

def variants(path,source):
 seed=parent(path,source)['energy1-mask1']
 a,z,fn=d.function(seed,'FPM_TONE_detect')
 old="""		filtered = sample;
		{
			short one = filtered;

			FPM_iir_filt_II(&one, iir_coeff, iir_state, 1, 1);
			filtered = one;
		}"""
 assert fn.count(old)==1
 fn=fn.replace(old,'\t\tFPM_iir_filt_II(&sample, iir_coeff, iir_state, 1, 1);').replace('int filtered, energy, in_band, excess;', 'int energy, in_band, excess;').replace('(filtered * filtered)', '((int)sample * sample)')
 return {'baseline':source,'prior-control':seed,'direct-sample':seed[:a]+fn+seed[z:]}

if __name__=='__main__':
 d.REV='8af3af53';d.SOURCE_PATHS=('src/dsp/fpm_tone.c',);d.OUT_NAME='next20-dsp-tone-sample';d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
