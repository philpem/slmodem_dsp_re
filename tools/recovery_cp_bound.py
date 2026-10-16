#!/usr/bin/env python3
"""Test original published bodyLength ownership of the CP encoder CRC bound."""
from pathlib import Path
import sys
import playbook_small_patterns as driver
from recovery_cp_crc import SOURCE, late_mask

driver.REV = 'd9995470'
driver.SOURCE_PATHS = (SOURCE,)
driver.OUT_NAME = 'recovery-cp-bound'
driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')


def variants(path, source):
    assert path == SOURCE
    source = late_mask(source, ['infoToBits', 'calcCRC', 'evaluateCRC'])
    start, end, body = driver.function(source, 'V90CP::infoToBits')
    old = '\tfor (i = 0x12; i < start; ) {'
    new = ('\tunsigned int end = bodyLength - 0x11;\n'
           '\tfor (i = 0x12; i < end; ) {')
    assert body.count(old) == 1
    candidate = source[:start] + body.replace(old, new) + source[end:]
    assert candidate != source
    return {'baseline': source, 'published-bound': candidate}


driver.variants = variants
if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    driver.main()
