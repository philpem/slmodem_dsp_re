#!/usr/bin/env python3
"""Replay supported unsigned mask clearing counters on the current complete TU."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_mechanism_constellation_reproduce import variants

if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '347b5b89'
    d.OUT_NAME = 'gcc3-constellation-counter-retention'
    d.SOURCE_PATHS = ('src/pump/v90/V90MappingParamsInt.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
