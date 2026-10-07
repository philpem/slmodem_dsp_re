#!/usr/bin/env python3
"""Test two coherent producer modes with ordinary default-double math."""
import playbook_small_patterns as d
from eia6_x87_type_reproduce import variants as typed


def variants(path, source):
    controls = typed(path, source)
    cells = {'baseline': source}
    for mode in ('sf', 'df'):
        text = controls[mode+'-expanded-unequal']
        cells[mode+'-coherent'] = text
        start, end, function = d.function(text, 'V90PreFilter::setParamEia6')
        assert function.count('10000.0f') == 1 and function.count('if (xf != 0.0f)') == 1
        function = function.replace('10000.0f', '10000.0').replace('if (xf != 0.0f)', 'if (xf != 0.0)')
        function = function.replace('x > 0.0f', 'x > 0.0').replace('fabsf(x)', 'fabs(x)')
        cells[mode+'-default-math'] = text[:start]+function+text[end:]
    return cells


if __name__ == '__main__':
    d.REV = '2ca02aec'
    d.SOURCE_PATHS = ('src/pump/v90/V90PreFilter.cpp',)
    d.OUT_NAME = 'eia6-x87-default-math'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fdump-translation-unit')
    d.variants = variants
    d.main()
