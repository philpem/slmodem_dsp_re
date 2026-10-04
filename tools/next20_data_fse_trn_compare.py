#!/usr/bin/env python3
"""Independent original halfword comparator after unsigned symbol shift."""
import playbook_small_patterns as d
from next20_data_fse_trn import variants as predecessors

def variants(path, source):
    parent = predecessors(path, source)['unsigned-symbol']
    a, z, fn = d.function(parent, 'FSE_decision_trn')
    old = 'c = (c >> 1) >= 1 ? 3 : 1;'
    assert fn.count(old) == 1
    new = 'c = (unsigned short)(c >> 1) >= 1 ? 3 : 1;'
    return {'baseline': source, 'unsigned-parent': parent,
            'halfword-comparator': parent[:a] + fn.replace(old, new) + parent[z:]}

if __name__ == '__main__':
    d.REV = '8af3af53'
    d.SOURCE_PATHS = ('src/pump/v32/V32dec.c',)
    d.OUT_NAME = 'next20-data-fse-trn-compare'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
