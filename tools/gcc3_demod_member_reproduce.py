#!/usr/bin/env python3
"""Test the original mapping-pointer reload after isAltRbs, no width change.

The coarse post-call sample screen is not a magnitude witness here: negation
precedes the decoder calls. Original isAltRbs successors reload this's ADID
pointer, while the retained local caches it across that call. Test that
independently visible source ownership boundary instead.
"""
import playbook_small_patterns as d

def variants(path, source):
    start, end, fn = d.function(source, 'V90Phase3Demodulator::twoLevelDemod')
    old = '\tV90AutoDigitalImpDetector *ad = autoDigitalImpDetector;\n'
    assert fn.count(old) == 1
    fn = fn.replace(old, '')
    assert fn.count('ad->') == 2 and fn.count('(ad, ucode)') == 2
    fn = fn.replace('ad->', 'autoDigitalImpDetector->')
    fn = fn.replace('(ad, ucode)', '(autoDigitalImpDetector, ucode)')
    return {'baseline': source, 'member-reload': source[:start]+fn+source[end:]}

if __name__ == '__main__':
    d.REV = '14769433'
    d.SOURCE_PATHS = ('src/pump/v90/V90Phase3Demodulator.cpp',)
    d.OUT_NAME = 'gcc3-demod-member'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
