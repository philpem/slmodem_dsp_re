#!/usr/bin/env python3
"""Prepare two pinned C++ inputs for observational stack-slot tracing."""
import playbook_small_patterns as d

def variants(path, source):
    start, end, fn = d.function(source, 'V90AutoDigitalImpDetector::updateUref')
    assert fn.count('+ 0.5f);') == 1
    return {'baseline': source, 'double-half': source[:start]+fn.replace('+ 0.5f);', '+ 0.5);')+source[end:]}

if __name__ == '__main__':
    d.REV = 'f2cdb66a'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-uref-stack-reproduction'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
