#!/usr/bin/env python3
"""New addition-mode discriminator for the closed F7846 Uref spelling family."""
import itertools
import playbook_small_patterns as d

def variants(path, source):
    cells = {}
    for pair, uref in itertools.product((False, True), repeat=2):
        text = source
        methods = []
        if pair:
            methods += ['updateLinMappMeanAndVar', 'updateLinMappMeanAndVarAlt']
        if uref:
            methods += ['updateUref']
        for name in methods:
            start, end, fn = d.function(text, 'V90AutoDigitalImpDetector::'+name)
            # Only the executable expression; historical commentary remains.
            old = '+ 0.5f);'
            assert fn.count(old) == 1
            text = text[:start] + fn.replace(old, '+ 0.5);') + text[end:]
        label = 'baseline' if not (pair or uref) else 'pair-%d-uref-%d' % (pair, uref)
        cells[label] = text
    return cells

if __name__ == '__main__':
    d.REV = '3dbb1c33'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-value-carriers-uref'
    d.DUMP_FLAGS = ('-dr', '-dc', '-dR', '-dS')
    d.variants = variants
    d.main()
