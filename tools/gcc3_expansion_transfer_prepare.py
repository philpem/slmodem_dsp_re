#!/usr/bin/env python3
"""Prepare unchanged complete-TU controls for expansion-temporary attribution."""
import sys
from pathlib import Path
import playbook_small_patterns as d

if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '3986addf'
    d.OUT_NAME = 'gcc3-expansion-transfer-prepare'
    d.SOURCE_PATHS = ('src/pump/v90/VpcmFloModem.cpp', 'src/dsp/FloatFIR.cpp')
    if '--agc' in sys.argv:
        sys.argv.remove('--agc')
        d.OUT_NAME = 'gcc3-expansion-transfer-agc-prepare'
        d.SOURCE_PATHS = ('src/pump/v90/V90Demodulator.cpp',)
    d.DUMP_FLAGS = ('-v','-save-temps','-da')
    d.variants = lambda path, source: {'baseline':source}
    d.main()
