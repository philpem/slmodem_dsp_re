#!/usr/bin/env python3
"""Test the witnessed conditional-arm bypass of the common parameter reload."""
import playbook_small_patterns as d
from eia6_magnitude_lifetime_reproduce import variants as prior


def variants(path, source):
    fixed = prior(path, source)['early-live-magnitude']
    start, end, function = d.function(fixed, 'V90PreFilter::setParamEia6')
    old = '\t}\n\n\tp = params;\n\tV90PW(p)[0x460 / 4]'
    assert function.count(old) == 1
    function = function.replace(old, '\t} else {\n\t\tp = params;\n\t}\n\n\tV90PW(p)[0x460 / 4]')
    return {'baseline': source, 'early-live-magnitude': fixed,
            'tail-reload-else': fixed[:start]+function+fixed[end:]}


if __name__ == '__main__':
    d.REV = '75e7ef4b'
    d.SOURCE_PATHS = ('src/pump/v90/V90PreFilter.cpp',)
    d.OUT_NAME = 'eia6-tail-reload'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fsched-verbose=5')
    d.variants = variants
    d.main()
