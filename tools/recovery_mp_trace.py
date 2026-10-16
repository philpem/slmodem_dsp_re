#!/usr/bin/env python3
"""Capture the integrated MP full-TU baseline through every period RTL stage."""
import sys
from pathlib import Path
import playbook_small_patterns as d

if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'd9995470'
    d.OUT_NAME = 'recovery-mp-next-trace'
    d.SOURCE_PATHS = ('src/pump/v90/V90MP.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = lambda path, source: {'baseline': source}
    d.main()
