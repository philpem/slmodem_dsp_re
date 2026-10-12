#!/usr/bin/env python3
"""Replay unchanged ADID TU with verbose second-scheduler diagnostics."""
import sys
from pathlib import Path
import playbook_small_patterns as d

if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '90b7f0d1'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-adid-first-study-schedule'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg', '-do', '-dw',
                    '-dz', '-dE', '-dn', '-dB', '-dR', '-dk', '-fsched-verbose=5')
    d.variants = lambda path, source: {'baseline': source}
    d.main()
