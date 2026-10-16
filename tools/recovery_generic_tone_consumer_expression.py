#!/usr/bin/env python3
"""Discriminate declaration-versus-arithmetic threshold operands before RTL.

GCC 3.4.2 fold-const.c tree_swap_operands_p swaps a declaration on the left
with a non-declaration on the right.  The original compares the shared mean
output product against the threshold in memory.  Keep that producer at its
comparison use and let CSE establish whether the product can remain shared.
"""
import playbook_small_patterns as driver


def variants(path, source):
    start = source.index('int GenericToneDetector::process(float sample)')
    end = source.index('\n}\n', start) + 2
    function = source[start:end]
    old = 'if (meanOut >= threshold)'
    assert function.count(old) == 1
    changed = function.replace(old, 'if (out * inv >= threshold)')
    return {'baseline': source,
            'product-at-threshold': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    driver.REV = 'acfa905a'
    driver.SOURCE_PATHS = ('src/dsp/GenericToneDetector.cpp',)
    driver.OUT_NAME = 'recovery-generic-tone-consumer-expression'
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fdump-tree-original')
    driver.variants = variants
    driver.main()
