#!/usr/bin/env python3
"""Prepare unchanged full PSD RTL and UID assembly with raw baseline validation."""
import sys
from pathlib import Path
import playbook_small_patterns as d


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '5cd55f7a'
    d.OUT_NAME = 'gcc3-psd-conversion-prepare'
    d.SOURCE_PATHS = ('src/dsp/psd.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dAP')
    if '--no-asm-annotations' in sys.argv:
        sys.argv.remove('--no-asm-annotations')
        d.OUT_NAME = 'gcc3-psd-conversion-plain'
        d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = lambda path,source: {'baseline':source}
    d.main()
