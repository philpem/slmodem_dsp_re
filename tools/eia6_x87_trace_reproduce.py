#!/usr/bin/env python3
"""Replay closed EIA6 controls with additional tree diagnostics."""
import playbook_small_patterns as d
from batch_cpp_prefilter_expand_reproduce import variants


if __name__ == '__main__':
    d.REV = '2ca02aec'
    d.SOURCE_PATHS = ('src/pump/v90/V90PreFilter.cpp',)
    d.OUT_NAME = 'eia6-x87-trace'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fdump-translation-unit')
    d.variants = variants
    d.main()
