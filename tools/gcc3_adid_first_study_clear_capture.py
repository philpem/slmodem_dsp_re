#!/usr/bin/env python3
"""Test original high-loop code capture before middle accumulator clearing."""
import sys
import re
from pathlib import Path
import playbook_small_patterns as d

def variants(path, source):
    a, z, fn = d.function(source, 'V90AutoDigitalImpDetector::porcessFirstStudy')
    before = '\t\tmagnitudeCount[phase][ucode] = 0;'
    init = 'for (k = (unsigned char)(ucode + 1); k < V90ADID_CODES; k++)'
    assert fn.count(before) == fn.count(init) == 1
    early = fn.replace(before, '\t\tk = ucode;\n\n'+before).replace(init,
                'for (k = (unsigned char)(k + 1); k < V90ADID_CODES; k++)')
    shared = early
    for field in ('magnitudeCount', 'magnitudeSum', 'magnitudeSqSum'):
        old = field+'[phase][ucode]'
        assert shared.count(old) == 1
        shared = shared.replace(old, field+'[phase][k]')
    split = fn.replace(before, '\t\tunsigned char upperCode = ucode;\n\n'+before)
    high = split.index(init)
    split = split[:high]+re.sub(r'\bk\b', 'upperCode', split[high:])
    split = split.replace('upperCode = (unsigned char)(ucode + 1)',
                          'upperCode = (unsigned char)(upperCode + 1)')
    return {'baseline': source, 'high-code-captured-before-clear': source[:a]+early+source[z:],
            'middle-and-high-share-captured-code': source[:a]+shared+source[z:],
            'separate-high-counter-captured-before-clear': source[:a]+split+source[z:]}

if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '90b7f0d1'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-adid-first-study-clear-capture'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg', '-do', '-dw',
                    '-dz', '-dE', '-dn', '-dB', '-dR', '-dk', '-fsched-verbose=5')
    d.variants = variants
    d.main()
