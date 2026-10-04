#!/usr/bin/env python3
"""V90Jd ctor local declaration order, scored on both clones.

Follows the v90jd-pos agent run (#22 5973342707/5973408651): position is
excluded; the blob's C1 is byte-identical to its C2 while ours diverges on
the second clone. Six permutations of the three local declarations; C2 must
stay EXACT in every cell. No fuzzing or mutation execution.
"""
import itertools
import playbook_small_patterns as driver

REV = 'c1617cf8'
OUT_NAME = 'playbook-v90jd-locals'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V90Jd.cpp',)

OLD = '\tunsigned char look;\n\tint mask;\n\tint i;\n'


def variants(path, source):
    assert source.count(OLD) == 1
    decls = ['\tunsigned char look;\n', '\tint mask;\n', '\tint i;\n']
    cells = {'baseline': source}
    for n, perm in enumerate(itertools.permutations(decls)):
        if perm == tuple(decls):
            continue
        cells['locals-%d' % n] = source.replace(OLD, ''.join(perm))
    assert len(cells) == 6
    return cells


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
