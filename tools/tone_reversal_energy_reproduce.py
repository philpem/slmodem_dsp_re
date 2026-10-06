#!/usr/bin/env python3
"""Test the original separately updated reversal-energy accumulator."""
import playbook_small_patterns as d


def variants(path, source):
    old = '\t\tenergy += ((samples[n] * samples[n]) >> 15)\n\t\t\t  - ((hist[idx] * hist[idx]) >> 15);'
    new = '\t\tenergy += (samples[n] * samples[n]) >> 15;\n\t\tenergy -= (hist[idx] * hist[idx]) >> 15;'
    assert source.count(old) == 1
    return {'baseline': source, 'separate-energy-updates': source.replace(old, new)}


if __name__ == '__main__':
    d.REV = 'e0052eec'
    d.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    d.OUT_NAME = 'tone-reversal-energy'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
