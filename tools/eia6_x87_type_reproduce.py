#!/usr/bin/env python3
"""Cross source producer precision with the witnessed EIA6 copy/predicate graph."""
import playbook_small_patterns as d
from batch_cpp_prefilter_expand_reproduce import variants as prior_variants


def variants(path, source):
    controls = prior_variants(path, source)
    result = {}
    for graph in ('baseline', 'expanded-unequal'):
        for precision in ('long double', 'float', 'double'):
            text = controls[graph]
            start, end, function = d.function(text, 'V90PreFilter::setParamEia6')
            if precision != 'long double':
                assert function.count('long double') == 3
                function = function.replace('long double', precision)
                function = function.replace('x > 0.0L', 'x > '+('0.0f' if precision == 'float' else '0.0'))
                function = function.replace('fabsl(x)', ('fabsf' if precision == 'float' else 'fabs')+'(x)')
            text = text[:start]+function+text[end:]
            result['baseline' if graph == 'baseline' and precision == 'long double' else
                   ('xf' if precision == 'long double' else 'sf' if precision == 'float' else 'df')+'-'+graph] = text
    return result


if __name__ == '__main__':
    d.REV = '2ca02aec'
    d.SOURCE_PATHS = ('src/pump/v90/V90PreFilter.cpp',)
    d.OUT_NAME = 'eia6-x87-type'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fdump-translation-unit')
    d.variants = variants
    d.main()
