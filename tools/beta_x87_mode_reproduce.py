#!/usr/bin/env python3
"""Transfer the independently observed live conversion mode to beta diagnostics."""
import playbook_small_patterns as d


def variants(path, source):
    cells = {'baseline': source}
    for precision, absolute in (('float', 'fabsf'), ('double', 'fabs')):
        text = source
        for method, scale in (('setLinearEquBeta', '1.0e10f'), ('setDfeBeta', '1.0e7f')):
            start, end, function = d.function(text, 'V90Equalizer::'+method)
            old = 'long double scaled = (long double)beta * '+scale+';'
            assert function.count(old) == 1 and function.count('fabsl(scaled)') == 1
            function = function.replace(old, precision+' scaled = ('+precision+')beta * '+scale+';')
            function = function.replace('fabsl(scaled)', absolute+'(scaled)')
            text = text[:start]+function+text[end:]
        cells[precision+'-diagnostic'] = text
    return cells


if __name__ == '__main__':
    d.REV = '2ca02aec'
    d.SOURCE_PATHS = ('src/pump/v90/V90Equalizer.cpp',)
    d.OUT_NAME = 'beta-x87-mode'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
