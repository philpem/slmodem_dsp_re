#!/usr/bin/env python3
"""Original FABS and DF fractional-scale witnesses in the timing diagnostic."""
import playbook_small_patterns as d


def variants(path, source):
    assert source.count('return (int)fabsf(v);') == 1
    assert source.count('long double x = v;\n\tlong double d = (int)v - x;') == 1
    cells = {}
    for whole in (False, True):
        for fraction in (False, True):
            modified = source
            if whole:
                modified = modified.replace('return (int)fabsf(v);', 'return (int)fabs(v);')
            if fraction:
                modified = modified.replace('long double x = v;\n\tlong double d = (int)v - x;',
                                            'double x = v;\n\tdouble d = (int)v - x;')
            label = 'baseline' if not (whole or fraction) else 'whole-double-%d-fraction-double-%d' % (whole, fraction)
            cells[label] = modified
    cells['baseline-repeat'] = source
    return cells


if __name__ == '__main__':
    d.REV = 'a95a6c65'
    d.SOURCE_PATHS = ('src/pump/v90/ResamplerTiming.cpp',)
    d.OUT_NAME = 'v90-timing-diagnostic-math'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
