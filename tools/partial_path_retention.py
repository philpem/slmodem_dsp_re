#!/usr/bin/env python3
"""Revalidate two existing supported partial path recoveries on current TUs."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from v32_cleaned_path_reproduce import variants as v32
from residual_v8_cursor_reproduce import variants as v8

def variants(path, source):
    if path.endswith('/V32.c'):
        return v32(path, source)
    cells = v8(path, source)
    return {label: cells[label] for label in ('baseline', 'ring-1-symbol-0')}

if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = 'd4c3ffea'
    d.SOURCE_PATHS = ('src/pump/v32/V32.c', 'src/v8/V8Interface.c')
    d.OUT_NAME = 'partial-path-retention'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
