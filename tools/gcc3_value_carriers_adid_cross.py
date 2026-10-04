#!/usr/bin/env python3
"""Cross independently witnessed half-offset precision in the ADID mean pair."""
import itertools
import playbook_small_patterns as d

def variants(path, source):
    cells = {}
    for ordinary, alternate in itertools.product((False, True), repeat=2):
        text = source
        for enabled, name in [(ordinary, 'updateLinMappMeanAndVar'), (alternate, 'updateLinMappMeanAndVarAlt')]:
            if enabled:
                start, end, fn = d.function(text, 'V90AutoDigitalImpDetector::'+name)
                assert fn.count('+ 0.5f)') == 1
                text = text[:start] + fn.replace('+ 0.5f)', '+ 0.5)') + text[end:]
        label = ('ordinary-' + str(int(ordinary)) + '-alternate-' + str(int(alternate))) if ordinary or alternate else 'baseline'
        cells[label] = text
    return cells

if __name__ == '__main__':
    d.REV = '3dbb1c33'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-value-carriers-adid-cross'
    d.DUMP_FLAGS = ('-dr', '-dc', '-dR', '-dS')
    d.variants = variants
    d.main()
