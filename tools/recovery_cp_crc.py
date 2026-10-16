#!/usr/bin/env python3
"""Replay independently witnessed late feedback masks in the complete CP TU."""
import itertools
from pathlib import Path
import sys
import playbook_small_patterns as driver

SOURCE = 'src/pump/v90/V90CP.cpp'
driver.REV = 'd9995470'
driver.SOURCE_PATHS = (SOURCE,)
driver.OUT_NAME = 'recovery-cp-crc'
driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')


def late_mask(source, methods):
    for method in methods:
        start, end, body = driver.function(source, 'V90CP::' + method)
        old = 'a = (unsigned char)((crc[0] + bits[i]) & 1);'
        new = 'a = (unsigned char)(crc[0] + bits[i]);'
        assert body.count(old) == body.count('crc[15] = a;') == 1
        body = body.replace(old, new).replace('crc[15] = a;',
                                              'crc[15] = (unsigned char)(a & 1);')
        source = source[:start] + body + source[end:]
    return source


def variants(path, source):
    assert path == SOURCE
    cells = {'baseline': source}
    for encoder, siblings in itertools.product((0, 1), repeat=2):
        if not (encoder or siblings):
            continue
        methods = (['infoToBits'] if encoder else [])
        methods += ['calcCRC', 'evaluateCRC'] if siblings else []
        cells['encoder-%d-siblings-%d' % (encoder, siblings)] = late_mask(source, methods)
    assert len(cells) == len(set(cells.values())) == 4
    return cells


driver.variants = variants
if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    driver.main()
