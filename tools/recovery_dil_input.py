#!/usr/bin/env python3
"""Test the measured byte-to-HI ceiling producer with capture controls."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_dil_topology import PATHS
from recovery_dil_padding import variants as padding
from recovery_dil_products import variants as products


def variants(path, source):
    v90 = 'V90DIL' in path
    base = padding(path, source)['first-1-second-1'] if v90 else products(path, source)['crc-before-info']
    cells = {'baseline': base}
    for byte, repeat in (itertools.product((0, 1), repeat=2) if v90 else ((0, 0), (1, 0))):
        if not (byte or repeat):
            continue
        text = base
        if repeat:
            text = products(path, source)['repeat-float']
        if byte:
            old = 'dilCeiling(short n, float reciprocal)' if v90 else 'dilCeiling(int n, float scale)'
            new = 'dilCeiling(unsigned char n, float reciprocal)' if v90 else 'dilCeiling(unsigned char n, float scale)'
            assert text.count(old) == 1
            text = text.replace(old, new)
            if v90:
                text = text.replace('(float)n', '(float)(short)n')
        cells['byte-%d-repeat-%d' % (byte, repeat)] = text
    assert len(cells) == len(set(cells.values())) == (4 if v90 else 2)
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-input'
    d.SOURCE_PATHS = PATHS
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
