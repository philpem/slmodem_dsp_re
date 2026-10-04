#!/usr/bin/env python3
"""Two bounded data-mode domains: v8_crc msb spelling and V92CP ctor order.

Posted as the "two fresh small data-mode domains" comment in #22. The v8_crc
cells change only the sign-bit extraction spelling (value-identical for all
16-bit inputs); the V92CP cells reorder independent stores. No fuzzing or
mutation execution; canonical verdicts only.
"""
import playbook_small_patterns as driver

REV = '2185e6b5'
OUT_NAME = 'playbook-v8crc-v92cp-ctor'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/v8/V8global.c', 'src/pump/v90/V92CP.cpp')

MSB_OLD = '\tint msb = ((int)(short)crc) < 0 ? 1 : 0;'

CTOR_BODY = '''	byte_04 = 0;

	rxState = 0;
	onesRun = 0;
	zerosRun = 0;
	bitIndex = 18;
	stateBitCount = 0;
'''


def variants(path, source):
    cells = {'baseline': source}
    if path == 'src/v8/V8global.c':
        assert source.count(MSB_OLD) == 1
        cells['msb-field-read'] = source.replace(
            MSB_OLD, '\tint msb = hs->crc < 0 ? 1 : 0;')
        cells['msb-shift-control'] = source.replace(
            MSB_OLD, '\tint msb = (hs->crc >> 15) & 1;')
    else:
        # resetDetector's statement order with byte_04 at each of the six
        # insertion points before word_914 (word_914 stays last).
        base = ['bitIndex = 18;', 'stateBitCount = 0;', 'rxState = 0;',
                'onesRun = 0;', 'zerosRun = 0;']
        for pos in range(6):
            order = base[:]
            order.insert(pos, 'byte_04 = 0;')
            body = '\t' + '\n\t'.join(order) + '\n'
            assert source.count(CTOR_BODY) == 1
            cells['ctor-order-%d' % pos] = source.replace(CTOR_BODY, body)
    assert len(cells) == len(set(cells.values()))
    return cells


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
