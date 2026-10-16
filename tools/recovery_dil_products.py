#!/usr/bin/env python3
"""Test original repeated-product graph and measured two-array home exchange."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_dil_topology import PATHS
from recovery_dil_padding import variants as padding


def variants(path, source):
    base = padding(path, source)['first-1-second-1']
    cells = {'baseline': base}
    if 'V90DIL' in path:
        old = ('\tfloat scaled = (float)n * reciprocal;\n\n'
               '\tif (scaled > 0.0f)\n\t\treturn (int)(scaled + 0.99999999);\n'
               '\treturn (int)scaled;')
        assert base.count(old) == 1
        for mode in ('float', 'double'):
            replacement = ('\tif ((float)n * reciprocal > 0.0f)\n'
                           '\t\treturn (int)((%s)n * reciprocal + 0.99999999);\n'
                           '\treturn (int)((float)n * reciprocal);') % mode
            cells['repeat-' + mode] = base.replace(old, replacement)
        assert len(cells) == len(set(cells.values())) == 3
    else:
        old = '\tunsigned char v92Info[19] = {'
        assert base.count(old) == 1 and base.count('\tint crc[16];') == 1
        text = base.replace('\tint crc[16];\n', '')
        cells['crc-before-info'] = text.replace(old, '\tint crc[16];\n' + old)
        assert len(cells) == len(set(cells.values())) == 2
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-products'
    d.SOURCE_PATHS = PATHS
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
