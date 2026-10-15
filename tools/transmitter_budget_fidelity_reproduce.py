#!/usr/bin/env python3
"""Replay only the witnessed budget conversion, keeping all other axes fixed."""
import playbook_small_patterns as d
from batch50_fax_budget import NAMES, changed


def variants(path, source):
    text = source
    for name in NAMES[path.rsplit('/', 1)[-1][:-2]]:
        a, z, fn = d.function(text, name)
        text = text[:a] + changed(fn, True, False) + text[z:]
    return {'baseline': source, 'unsigned-budget': text}


if __name__ == '__main__':
    d.REV = '1b0a3097'
    d.SOURCE_PATHS = ('src/fax/V17t_prc.c', 'src/fax/V29t_prc.c')
    d.OUT_NAME = 'transmitter-budget-fidelity'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
