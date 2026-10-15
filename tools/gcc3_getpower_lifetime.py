#!/usr/bin/env python3
"""Cross double declaration scope after observed reload slot allocation."""
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    a, z, fn = d.function(source, 'V90ConstellationPower::getPower')
    assert fn.count('\t\t\tdouble fraction;\n') == 1
    lifted = fn.replace('\t\t\tdouble fraction;\n', '')
    cells = {'baseline': source}
    for first in (False, True):
        declarations = '\tdouble fraction;\n\tdouble power;' if first else '\tdouble power;\n\tdouble fraction;'
        text = lifted.replace('\tdouble power;', declarations)
        cells['fraction-first-%d' % first] = source[:a] + text + source[z:]
    assert len(cells) == len(set(cells.values())) == 3
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'f420cd89'
    d.SOURCE_PATHS = ('src/pump/v90/V90ConstellationPower.cpp',)
    d.OUT_NAME = 'gcc3-getpower-lifetime'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
