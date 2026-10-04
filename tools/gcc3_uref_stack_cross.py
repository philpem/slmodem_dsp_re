#!/usr/bin/env python3
"""Cross Uref expression mode with independently observed callee NaN materialization."""
import itertools
import playbook_small_patterns as d

def variants(path, source):
    cells = {}
    for constant, rounding in itertools.product((False, True), repeat=2):
        text = source
        if constant:
            start, end, fn = d.function(text, 'V90AutoDigitalImpDetector::unitePhasesInfoOfUref')
            assert fn.count('float bestVar = nanf("");') == 1
            text = text[:start]+fn.replace('float bestVar = nanf("");', 'float bestVar = NAN;')+text[end:]
        if rounding:
            start, end, fn = d.function(text, 'V90AutoDigitalImpDetector::updateUref')
            assert fn.count('+ 0.5f);') == 1
            text = text[:start]+fn.replace('+ 0.5f);', '+ 0.5);')+text[end:]
        label = 'baseline' if not (constant or rounding) else 'constant-%d-rounding-%d' % (constant, rounding)
        cells[label] = text
    return cells

if __name__ == '__main__':
    d.REV = 'f2cdb66a'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-uref-stack-cross'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
