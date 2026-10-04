#!/usr/bin/env python3
"""Four controls for the second originally constant NaN sentinel in ADID."""
import itertools
import playbook_small_patterns as d

def variants(path, source):
    cells = {}
    for predecessor, second in itertools.product((False, True), repeat=2):
        text = source
        if predecessor:
            start, end, fn = d.function(text, 'V90AutoDigitalImpDetector::unitePhasesInfoOfUref')
            assert fn.count('float bestVar = nanf("");') == 1
            text = text[:start]+fn.replace('float bestVar = nanf("");', 'float bestVar = NAN;')+text[end:]
            start, end, fn = d.function(text, 'V90AutoDigitalImpDetector::updateUref')
            assert fn.count('+ 0.5f);') == 1
            text = text[:start]+fn.replace('+ 0.5f);', '+ 0.5);')+text[end:]
        if second:
            start, end, fn = d.function(text, 'V90AutoDigitalImpDetector::porcessSecondStudy')
            assert fn.count('float second = nanf("");') == 1
            text = text[:start]+fn.replace('float second = nanf("");', 'float second = NAN;')+text[end:]
        label = 'baseline' if not (predecessor or second) else 'predecessor-%d-second-%d' % (predecessor, second)
        cells[label] = text
    return cells

if __name__ == '__main__':
    d.REV = 'f2cdb66a'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-uref-nan-second'
    d.DUMP_FLAGS = ('-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
