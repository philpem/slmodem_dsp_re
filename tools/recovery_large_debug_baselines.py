#!/usr/bin/env python3
"""Freeze raw complete-TU controls for three larger diagnostic nominations."""
import playbook_small_patterns as d

d.REV = 'f8c8225c'
d.OUT_NAME = 'large-diagnostics-next'
d.SOURCE_PATHS = ('src/v8/V8.c', 'src/pump/v90/V90Demodulator.cpp', 'src/pump/v34/V34hshak.c')
d.DUMP_FLAGS = ('-da', '-fdump-tree-original')
d.variants = lambda path, source: {'baseline': source}
if __name__ == '__main__':
    d.main()
