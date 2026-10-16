#!/usr/bin/env python3
"""Test the original floating predicate before its integer-conversion boundary."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_dil_input import variants as inputs

PATH = 'src/pump/v90/V90DILdesPCK.cpp'


def variants(path, source):
    base = inputs(path, source)['byte-1-repeat-0']
    cells = {'baseline': base}
    old = ('\tfloat scaled = (float)(short)n * reciprocal;\n\n'
           '\tif (scaled > 0.0f)\n\t\treturn (int)(scaled + 0.99999999);\n'
           '\treturn (int)scaled;')
    assert base.count(old) == 1
    cells['outer-cached'] = base.replace(old, '\tfloat scaled = (float)(short)n * reciprocal;\n\n\treturn (int)(scaled > 0.0f ? scaled + 0.99999999 : scaled);')
    for mode in ('float', 'double'):
        replacement = ('\treturn (int)((float)(short)n * reciprocal > 0.0f\n'
                       '\t\t? (%s)(short)n * reciprocal + 0.99999999\n'
                       '\t\t: (float)(short)n * reciprocal);') % mode
        cells['outer-repeat-' + mode] = base.replace(old, replacement)
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-conversion'
    d.SOURCE_PATHS = (PATH,)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
