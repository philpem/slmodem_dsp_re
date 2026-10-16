#!/usr/bin/env python3
"""Cross original scalar payload bounds and signed byte dispatch in full CP TU."""
import itertools
from pathlib import Path
import sys
import playbook_small_patterns as driver
from recovery_cp_crc import SOURCE
from recovery_cp_captures import variants as captures_variants

driver.REV = 'd9995470'
driver.SOURCE_PATHS = (SOURCE,)
driver.OUT_NAME = 'recovery-cp-storage'
driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')


def variants(path, source):
    source = captures_variants(path, source)['four-captures']
    start, end, body = driver.function(source, 'V90CP::infoToBits')
    cells = {'baseline': source}
    for scalar, byte in itertools.product((0, 1), repeat=2):
        if not (scalar or byte):
            continue
        changed = body
        if scalar:
            old = 'unsigned int n[4];'
            assert changed.count(old) == 1
            changed = changed.replace(old, 'unsigned int n0, n1, n2, n3;')
            for k in range(4):
                assert changed.count('n[%d]' % k) == 2
                changed = changed.replace('n[%d]' % k, 'n%d' % k)
        if byte:
            assert changed.count('switch (byte_11)') == 1
            changed = changed.replace('switch (byte_11)', 'switch ((signed char)byte_11)')
        cells['scalar-%d-byte-%d' % (scalar, byte)] = source[:start] + changed + source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


driver.variants = variants
if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    driver.main()
