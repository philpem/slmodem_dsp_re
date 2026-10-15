#!/usr/bin/env python3
"""Prepare unchanged full-TU stack observations for the getPower slot exchange."""
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    return {'baseline': source}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'f420cd89'
    d.SOURCE_PATHS = ('src/pump/v90/V90ConstellationPower.cpp',)
    d.OUT_NAME = 'gcc3-getpower-stack'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
