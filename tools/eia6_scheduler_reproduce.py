#!/usr/bin/env python3
"""Replay fixed EIA6 controls with scheduler diagnostics, without new source cells."""
import playbook_small_patterns as d
from eia6_x87_default_math_reproduce import variants as prior


def variants(path, source):
    cells = prior(path, source)
    return {label: cells[label] for label in ('baseline', 'sf-default-math')}


if __name__ == '__main__':
    d.REV = '75e7ef4b'
    d.SOURCE_PATHS = ('src/pump/v90/V90PreFilter.cpp',)
    d.OUT_NAME = 'eia6-scheduler'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fsched-verbose=5')
    d.variants = variants
    d.main()
