#!/usr/bin/env python3
"""Prepare unchanged complete TUs for the two witnessed SIZE spill exchanges."""
import sys
from pathlib import Path
import playbook_small_patterns as d


if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV='b6db3cfc'
    d.OUT_NAME='gcc3-size-spill-prepare'
    d.SOURCE_PATHS=('src/pump/v90/V90MappingParamsInt.cpp','src/dsp/psd.cpp')
    d.DUMP_FLAGS=('-v','-save-temps','-dr','-dc','-dg')
    d.variants=lambda path,source:{'baseline':source}
    d.main()
