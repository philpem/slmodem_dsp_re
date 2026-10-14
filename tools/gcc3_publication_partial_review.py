#!/usr/bin/env python3
"""Two fixed partial publication controls, without inline/profile changes."""
import playbook_small_patterns as d
from tone_reversal_energy_reproduce import variants as energy
from batch_cpp_iir_reproduce import variants as iir


def variants(path,source):
    if path.endswith('/fpm_tone.c'):return energy(path,source)
    old=iir(path,source)
    return {k:old[k] for k in ('baseline','member-acc')}


if __name__=='__main__':
    d.REV='ec575202'
    d.SOURCE_PATHS=('src/dsp/fpm_tone.c','src/dsp/FloatIIR.cpp')
    d.OUT_NAME='gcc3-publication-partial-review'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
