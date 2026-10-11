#!/usr/bin/env python3
"""Cross ADID first-study helper factoring with counter declaration age."""
import sys
from pathlib import Path
import playbook_small_patterns as d

NAME = 'V90AutoDigitalImpDetector::porcessFirstStudy'


def variants(path, source):
    a, z, fn = d.function(source, NAME)
    loop = '\t\t\tfloat inv, mean;\n\n\t\t\tif (ucode == code)\n\t\t\t\tcontinue;\n\t\t\tif (magnitudeCount[phase][code] == 0)\n\t\t\t\tcontinue;\n\n\t\t\tinv = 1.0f / magnitudeCount[phase][code];\n\t\t\tmean = magnitudeSum[phase][code] * inv;\n\n\t\t\tlinearMappingVar[phase][code] =\n\t\t\t    inv * magnitudeSqSum[phase][code] - mean * mean;\n\t\t\tlinMapp[phase][code] = (short)(mean + 0.5f);'
    assert fn.count(loop) == 1
    forms = {'explicit-float': fn,
             'explicit-double': fn.replace('(short)(mean + 0.5f)', '(short)(mean + 0.5)'),
             'helper': fn.replace(loop, '\t\t\tif (ucode == code)\n\t\t\t\tcontinue;\n\t\t\tupdateLinMappMeanAndVar(phase, code);')}
    cells = {'baseline': source}
    for label, body in forms.items():
        for late in (False, True):
            if label == 'explicit-float' and not late:
                continue
            text = body
            if late:
                declaration = '\tunsigned char phase;\n\tunsigned char code;\n'
                assert text.count(declaration) == 1
                text = text.replace(declaration, '')
                marker = '\tfor (phase = 0; phase < V90ADID_PHASES; phase++) {'
                text = text.replace(marker, declaration+'\n'+marker, 1)
            cells[label+'-late-%d' % late] = source[:a]+text+source[z:]
    assert len(cells) == len(set(cells.values())) == 6
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '9aa118e1'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-adid-first-study'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
