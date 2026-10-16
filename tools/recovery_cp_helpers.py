#!/usr/bin/env python3
"""Cross existing CP helper ownership for corresponding original inline regions."""
import itertools
from pathlib import Path
import sys
import playbook_small_patterns as driver
from recovery_cp_crc import SOURCE
from recovery_cp_storage import variants as storage_variants

driver.REV = 'd9995470'
driver.SOURCE_PATHS = (SOURCE,)
driver.OUT_NAME = 'recovery-cp-helpers'
driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')


def variants(path, source):
    source = storage_variants(path, source)['scalar-1-byte-1']
    start, end, body = driver.function(source, 'V90CP::infoToBits')
    cells = {'baseline': source}
    for reset, crc, length in itertools.product((0, 1), repeat=3):
        if not (reset or crc or length):
            continue
        changed = body
        if reset:
            old = '\tfor (c = 0; c <= 0xf; c++)\n\t\tcrc[c] = 1;'
            assert changed.count(old) == 1
            changed = changed.replace(old, '\tresetCRC();')
        if crc:
            first = changed.index('\tunsigned int end = bodyLength - 0x11;')
            last = changed.index('\n\tpos = (unsigned int)bitIndex;', first)
            changed = changed[:first] + '\tcalcCRC();\n' + changed[last:]
        if length:
            first = changed.index('\ttotal = nbits + 1;')
            last = changed.index('\n\tpos = (unsigned int)bitIndex;', first)
            changed = changed[:first] + '\tcalcSequenceLength();\n' + changed[last:]
        cells['reset-%d-crc-%d-length-%d' % (reset, crc, length)] = source[:start] + changed + source[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


driver.variants = variants
if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    driver.main()
