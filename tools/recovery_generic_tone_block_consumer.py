#!/usr/bin/env python3
"""Test the independently witnessed block threshold producer/consumer graph."""
import playbook_small_patterns as driver


def variants(path, source):
    start = source.index('int GenericToneDetector::process(float *samples, unsigned int n)')
    end = source.index('\n}\n', start) + 2
    function = source[start:end]
    old = 'if (meanOut >= threshold)'
    assert function.count(old) == 1
    changed = function.replace(old, 'if (out * inv >= threshold)')
    return {'baseline': source,
            'block-product-at-threshold': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    driver.REV = 'd127aec8'
    driver.SOURCE_PATHS = ('src/dsp/GenericToneDetector.cpp',)
    driver.OUT_NAME = 'recovery-generic-tone-block-consumer'
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fdump-tree-original')
    driver.variants = variants
    driver.main()
