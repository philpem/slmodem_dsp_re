#!/usr/bin/env python3
"""Attribute the residual block countdown copy; option cell is diagnostic only."""
import playbook_small_patterns as driver


if __name__ == '__main__':
    driver.REV = '37a77c90'
    driver.SOURCE_PATHS = ('src/dsp/GenericToneDetector.cpp',)
    driver.OUT_NAME = 'recovery-generic-tone-bbro'
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fdump-tree-original')
    driver.variants = lambda path, source: {'baseline': source, 'no-reorder-blocks': source}
    driver.CELL_FLAGS = lambda path, label: ('-fno-reorder-blocks',) if label == 'no-reorder-blocks' else ()
    driver.main()
