#!/usr/bin/env python3
"""Bound fractional absolute-value uses after witnessing combine sink its FIX."""
import playbook_small_patterns as d
from eia6_x87_default_math_reproduce import variants as prior


def variants(path, source):
    fixed = prior(path, source)['sf-default-math']
    start, end, function = d.function(fixed, 'V90PreFilter::setParamEia6')
    old = '(frac < 0) ? -frac : frac'
    assert function.count(old) == 1
    builtin = function.replace(old, '__builtin_abs(frac)')
    conditional = function.replace('\txf = (float)x;', '\tif (frac < 0)\n\t\tfrac = -frac;\n\txf = (float)x;').replace(old, 'frac')
    cells = {'baseline': source, 'sf-default-math': fixed,
             'fraction-builtin-abs': fixed[:start]+builtin+fixed[end:],
             'fraction-explicit-if': fixed[:start]+conditional+fixed[end:]}
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = '75e7ef4b'
    d.SOURCE_PATHS = ('src/pump/v90/V90PreFilter.cpp',)
    d.OUT_NAME = 'eia6-fraction-use'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fsched-verbose=5')
    d.variants = variants
    d.main()
