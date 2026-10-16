#!/usr/bin/env python3
"""Cross original MP Type acquisition and absolute CRC publication boundaries."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_bitpacker_encoder import PATH, variants as encoder_cells


def variants(path, source):
    base = encoder_cells(path, source)['split-1-wide-1']
    cells = {'baseline': base}
    for late, absolute in itertools.product((0, 1), repeat=2):
        if not (late or absolute):
            continue
        start, end, fn = d.function(base, 'V90MP::infoToBits')
        if late:
            assert fn.count('unsigned char t = (unsigned char)Type;') == 1
            fn = fn.replace('unsigned char t = (unsigned char)Type;', 'unsigned char t;')
            fn = fn.replace('\tbits[0x11] = 0;', '\tt = (unsigned char)Type;\n\tbits[0x11] = 0;', 1)
        if absolute:
            old = '\t\tfor (k = 0; k <= 15; k++)\n\t\t\tbits[end + 1 + k] = crc[k];'
            assert fn.count(old) == 2
            for first, last in ((0xab, 0xba), (0x45, 0x54)):
                new = '\t\tfor (i = 0x%x; i <= 0x%x; i++)\n\t\t\tbits[i] = crc[i - 0x%x];' % (first, last, first)
                fn = fn.replace(old, new, 1)
        cells['late-%d-absolute-%d' % (late, absolute)] = base[:start] + fn + base[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'ea95b278'
    d.OUT_NAME = 'recovery-bitpacker-publication'
    d.SOURCE_PATHS = (PATH,)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
