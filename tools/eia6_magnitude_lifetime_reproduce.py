#!/usr/bin/env python3
"""Test the original early FABS/live magnitude graph after fraction-use tracing."""
import playbook_small_patterns as d
from eia6_fraction_use_reproduce import variants as prior


def variants(path, source):
    fixed = prior(path, source)['fraction-builtin-abs']
    start, end, function = d.function(fixed, 'V90PreFilter::setParamEia6')
    assert function.count('\twhole = (int)x;') == 1
    assert function.count('(int)fabs(x)') == 1
    function = function.replace('\twhole = (int)x;', '\twhole = (int)x;\n\tdouble magnitude = fabs(x);')
    function = function.replace('(int)fabs(x)', '(int)magnitude')
    return {'baseline': source, 'fraction-builtin-abs': fixed,
            'early-live-magnitude': fixed[:start]+function+fixed[end:]}


if __name__ == '__main__':
    d.REV = '75e7ef4b'
    d.SOURCE_PATHS = ('src/pump/v90/V90PreFilter.cpp',)
    d.OUT_NAME = 'eia6-magnitude-lifetime'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fsched-verbose=5')
    d.variants = variants
    d.main()
