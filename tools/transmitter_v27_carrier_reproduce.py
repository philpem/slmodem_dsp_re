#!/usr/bin/env python3
"""Cross two original Quiet value-use boundaries after the budget correction."""
import itertools
import playbook_small_patterns as d
from transmitter_v27_budget_reproduce import variants as budget


def variants(path, source):
    fixed = budget(path, source)['unsigned-budget']
    a, z, fn = d.function(fixed, 'TxHdxQuietV27')
    cells = {}
    for chosen, member in itertools.product((False, True), repeat=2):
        text = fn
        if chosen:
            assert text.count('\tshort taken;') == 1
            text = text.replace('\tshort taken;', '\tunsigned short taken;')
        if member:
            old = 'prm->countdown = (short)(countdown - taken);'
            assert text.count(old) == 1
            text = text.replace(old, 'prm->countdown -= taken;')
        label = '-'.join(k for k,v in [('unsigned-chosen',chosen),('member-decrement',member)] if v) or 'baseline'
        cells[label] = fixed[:a]+text+fixed[z:]
    return cells


if __name__ == '__main__':
    d.REV = '1b0a3097'
    d.SOURCE_PATHS = ('src/fax/V27t_prc.c',)
    d.OUT_NAME = 'transmitter-v27-carrier'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
