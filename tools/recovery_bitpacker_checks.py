#!/usr/bin/env python3
"""Cross original integer-abs graph with unsigned CRC-check iteration."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_bitpacker_boundaries import variants as boundaries

PATH = 'src/pump/v90/V90MP.cpp'


def variants(path, source):
    base = boundaries(path, source)['axis-1-1']
    cells = {'baseline': base}
    for absolute, unsigned in itertools.product((0, 1), repeat=2):
        if not (absolute or unsigned):
            continue
        text = base
        start, end, fn = d.function(text, 'V90MP::bitsToInfo')
        if absolute:
            old = 'sum += (unsigned char)(d < 0 ? -d : d);'
            assert fn.count(old) == 2
            fn = fn.replace(old, 'sum += (unsigned char)abs(d);')
        if unsigned:
            old = ('\t\tfor (k = 0; k <= 15; k++) {\n'
                   '\t\t\tint d = (int)crc[k] - (int)bits[n - 0x10 + k];')
            assert fn.count(old) == 2
            fn = fn.replace(old, '\t\tfor (i = 0; i <= 15; i++) {\n\t\t\tint d = (int)crc[i] - (int)bits[n - 0x10 + i];')
        text = text[:start] + fn + text[end:]
        if absolute:
            assert text.count('#include <stddef.h>') == 1
            text = text.replace('#include <stddef.h>', '#include <stddef.h>\n#include <stdlib.h>')
        cells['abs-%d-unsigned-%d' % (absolute, unsigned)] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'ea95b278'
    d.OUT_NAME = 'recovery-bitpacker-checks'
    d.SOURCE_PATHS = (PATH,)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
