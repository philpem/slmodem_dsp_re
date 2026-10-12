#!/usr/bin/env python3
"""Compare unchanged V32 getter with one diagnostic alias-option control."""
import sys
from pathlib import Path
import playbook_small_patterns as d


if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV='f420cd89'
    d.OUT_NAME='v32-cleaned-alias'
    d.SOURCE_PATHS=('src/pump/v32/V32.c',)
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=lambda path,source:{'baseline':source,'no-strict-diagnostic':source}
    d.CELL_FLAGS=lambda path,label:('-fno-strict-aliasing',) if label=='no-strict-diagnostic' else ()
    d.main()
