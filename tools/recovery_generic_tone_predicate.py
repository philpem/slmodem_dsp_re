#!/usr/bin/env python3
"""Discriminate the witnessed direct scalar threshold branch without a bool."""
import playbook_small_patterns as driver


def variants(path, source):
    start = source.index('int GenericToneDetector::process(float sample)')
    end = source.index('\n}\n', start) + 2
    function = source[start:end]
    old = 'if (meanOut >= threshold)'
    assert function.count(old) == 1
    replacement = function.replace(old, 'if (!(meanOut < threshold))')
    return {'baseline': source,
            'complementary-lower-bound': source[:start] + replacement + source[end:]}


if __name__ == '__main__':
    driver.REV = 'a2d88616'
    driver.SOURCE_PATHS = ('src/dsp/GenericToneDetector.cpp',)
    driver.OUT_NAME = 'recovery-generic-tone-predicate'
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    driver.variants = variants
    driver.main()
