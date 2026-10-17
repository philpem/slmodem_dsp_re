#!/usr/bin/env python3
"""Replay original log10 extent separately from the fixture link mode."""
import playbook_small_patterns as d
from stage_c_fpm_extents import recover_log

d.REV = '95e01c27'
d.SOURCE_PATHS = ('src/dsp/fpm_log10.c',)
d.OUT_NAME = 'stage-c-log10-extent'
d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
d.variants = lambda path, source: {'baseline':source, 'original-extent':recover_log(source)}
d.main()
