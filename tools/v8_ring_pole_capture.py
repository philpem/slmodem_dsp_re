#!/usr/bin/env python3
"""Cross the measured ring join with original pole-before-input capture."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from residual_v8_cursor_reproduce import variants as prior

def variants(path, source):
    parents = prior(path, source)
    cells = {'baseline': source, 'ring-1-pole-0': parents['ring-1-symbol-0']}
    for ring, parent in ((0, source), (1, parents['ring-1-symbol-0'])):
        a, z, fn = d.function(parent, 'V8Process')
        anchor = '\t\tv->tx_ring_base = tx;' if ring else '\t\tx = *in++;'
        assert fn.count(anchor) == 1
        fn = fn.replace(anchor, '\t\tunsigned short pole = (unsigned short)v->pole_state;\n'+anchor)
        operand = '(unsigned short)v->pole_state'
        assert fn.count(operand) == 2
        fn = fn.replace('x + '+operand, 'x + pole')
        cells['ring-%d-pole-1' % ring] = parent[:a]+fn+parent[z:]
    return cells

if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = 'd4c3ffea'
    d.SOURCE_PATHS = ('src/v8/V8Interface.c',)
    d.OUT_NAME = 'v8-ring-pole-capture'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
