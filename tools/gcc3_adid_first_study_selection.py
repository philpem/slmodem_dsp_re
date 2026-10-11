#!/usr/bin/env python3
"""Test observed count initialization and separate flag-publication arms."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_adid_first_study_combined import variants as prior
from gcc3_adid_first_study import NAME


def variants(path, source):
    parent = prior(path, source)['diagnostic-double-upper-guard']
    a, z, fn = d.function(parent, NAME)
    declaration = '\t\tshort n = 1;\n\n\t\tif (altRbsFlag[phase] == 0) {'
    assert fn.count(declaration) == 1
    late = fn.replace(declaration, '\t\tif (altRbsFlag[phase] == 0) {\n\t\t\tshort n = 1;')
    tail = '\t\t\tif (n > 9) {\n\t\t\t\tbyte_280c[phase] = 0;\n\t\t\t\tcontinue;\n\t\t\t}\n\t\t}\n\n\t\tsuspectedPhaseCount++;\n\t\tbyte_280c[phase] = 1;'
    assert late.count(tail) == 1
    nested = late.replace(tail, '\t\t\tif (n > 9) {\n\t\t\t\tbyte_280c[phase] = 0;\n\t\t\t} else {\n\t\t\t\tsuspectedPhaseCount++;\n\t\t\t\tbyte_280c[phase] = 1;\n\t\t\t}\n\t\t} else {\n\t\t\tsuspectedPhaseCount++;\n\t\t\tbyte_280c[phase] = 1;\n\t\t}')
    return {'baseline': source, 'combined': parent,
            'combined-count-inside': parent[:a]+late+parent[z:],
            'combined-nested-selection': parent[:a]+nested+parent[z:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '9aa118e1'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-adid-first-study-selection'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
