#!/usr/bin/env python3
"""Cross original full-width CRC end with its post-reset acquisition."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_bitpacker_checks import PATH, variants as checks


def variants(path, source):
    base = checks(path, source)['abs-1-unsigned-1']
    cells = {'baseline': base}
    for wide, late in itertools.product((0, 1), repeat=2):
        if not (wide or late):
            continue
        start, end, fn = d.function(base, 'V90MP::bitsToInfo')
        if wide:
            assert fn.count('\tunsigned int i;') == 1 and fn.count('x, sum, n, next, end;') == 1
            fn = fn.replace('\tunsigned int i;', '\tunsigned int i, end;').replace('x, sum, n, next, end;', 'x, sum, n, next;')
        if late:
            old = ('\t\tend = type ? 0xaa : 0x44;\n\n'
                   '\t\tfor (k = 0; k <= 15; k++)\n\t\t\tcrc[k] = 1;')
            assert fn.count(old) == 1
            new = ('\t\tfor (k = 0; k <= 15; k++)\n\t\t\tcrc[k] = 1;\n\n'
                   '\t\tend = type ? 0xaa : 0x44;')
            fn = fn.replace(old, new)
        cells['wide-%d-late-%d' % (wide, late)] = base[:start] + fn + base[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'ea95b278'
    d.OUT_NAME = 'recovery-bitpacker-end'
    d.SOURCE_PATHS = (PATH,)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
