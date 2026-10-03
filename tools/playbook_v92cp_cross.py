#!/usr/bin/env python3
"""V92CP ctor: decoded body order crossed with the register-productive positions.

Follows agent run ctor-tu-position (#22 5971674845/5971702309): position
alone co-locates the 0x12/-1 pair; the blob splits them. Hypothesis: the
split comes from the decoded statement order (F11672) ending 0x12's live
range before -1 is materialized, crossed with a position after reset.
No fuzzing or mutation execution.
"""
import playbook_small_patterns as driver

REV = '438af9b7'
OUT_NAME = 'playbook-v92cp-cross'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V92CP.cpp',)

OLD_BODY = '''	byte_04 = 0;

	rxState = 0;
	onesRun = 0;
	zerosRun = 0;
	bitIndex = 18;
	stateBitCount = 0;
'''
NEW_BODY = '''	bitIndex = 18;
	stateBitCount = 0;
	rxState = 0;
	onesRun = 0;
	zerosRun = 0;
	byte_04 = 0;
'''


def variants(path, source):
    assert source.count(OLD_BODY) == 1
    ordered = source.replace(OLD_BODY, NEW_BODY)
    cells = {'baseline': source, 'ordered-body': ordered}
    # move the ctor definition after V92CP::reset's definition
    ctor_start = ordered.index('V92CP::V92CP()')
    # segment = the ctor's banner comment (if any) plus definition
    seg_start = ordered.rindex('/*', 0, ctor_start)
    ctor_end = ordered.index('\n}\n', ctor_start) + 3
    seg = ordered[seg_start:ctor_end]
    reset_end = ordered.index('\n}\n', ordered.index('V92CP::reset()')) + 3
    moved = (ordered[:seg_start] + ordered[ctor_end:reset_end] + '\n' + seg +
             ordered[reset_end:])
    cells['ordered-after-reset'] = moved
    assert len(cells) == 3
    return cells


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
