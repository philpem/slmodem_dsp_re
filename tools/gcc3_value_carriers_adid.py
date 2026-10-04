#!/usr/bin/env python3
"""Two full-TU controls for an observed x87 half-offset expression mode."""
import playbook_small_patterns as d

def variants(path, source):
    start, end, fn = d.function(source, 'V90AutoDigitalImpDetector::updateLinMappMeanAndVarAlt')
    assert fn.count('+ 0.5f)') == 1
    changed = fn.replace('+ 0.5f)', '+ 0.5)')
    return {'baseline': source, 'double-half': source[:start] + changed + source[end:]}

if __name__ == '__main__':
    d.REV = '3dbb1c33'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-value-carriers-adid'
    d.DUMP_FLAGS = ('-dr', '-dc', '-dR', '-dS')
    d.variants = variants
    d.main()
