#!/usr/bin/env python3
"""Test witnessed wide product and direct shifted-CRC update graphs."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_dil_topology import PATHS
from recovery_dil_boundaries import variants as boundaries


def variants(path, source):
    v90 = 'V90DIL' in path
    label = 'narrow-1-reread-1-crc-1' if v90 else 'narrow-0-reread-0-crc-1'
    base = boundaries(path, source)[label]
    cells = {'baseline': base}
    for wide, direct in (itertools.product((0, 1), repeat=2) if v90 else ((0, 0), (0, 1))):
        if not (wide or direct):
            continue
        text = base
        if wide:
            assert text.count('float scaled = (float)n * reciprocal;') == 1
            text = text.replace('float scaled = (float)n * reciprocal;', 'double scaled = (double)n * reciprocal;')
        if direct:
            old = ('\tint stage3 = (crc[4] + feedback) & 1;\n'
                   '\tint stage10 = (crc[11] + feedback) & 1;\n'
                   '\tint stage15 = feedback & 1;\n')
            assert text.count(old) == 1
            text = text.replace(old, '')
            old = '\tcrc[3] = stage3;\n\tcrc[10] = stage10;\n\tcrc[15] = stage15;'
            assert text.count(old) == 1
            text = text.replace(old, '\tcrc[3] = (crc[3] + feedback) & 1;\n\tcrc[10] = (crc[10] + feedback) & 1;\n\tcrc[15] = feedback & 1;')
        cells['wide-%d-direct-%d' % (wide, direct)] = text
    assert len(cells) == len(set(cells.values())) == (4 if v90 else 2)
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-graph'
    d.SOURCE_PATHS = PATHS
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
