#!/usr/bin/env python3
"""Isolate the original middle info pointer while preserving indexed neighbours."""
from itertools import product
import playbook_small_patterns as d
from recovery_dil_next_info import REV, variants as info


def variants(path, source):
    prepared = info(path, source)
    cells = {}
    for pointer, parity in product((0, 1), repeat=2):
        text = prepared['base-1-even-%d' % parity]
        if pointer:
            old = '''\t\tfor (i = 16; i < 19; i++)
\t\t\tinfo[1 + i] = v92Info[i];'''
            new = '''\t\tunsigned char *tail = info + 17;
\t\tfor (i = 16; i < 19; i++)
\t\t\t*tail++ = v92Info[i];'''
            assert text.count(old) == 1
            text = text.replace(old, new)
        label = 'baseline' if not (pointer or parity) else 'tail-%d-even-%d' % (pointer, parity)
        cells[label] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = REV
    d.OUT_NAME = 'recovery-dil-next-tail'
    d.SOURCE_PATHS = ('src/pump/v90/V92DILdesPCK.cpp',)
    d.variants = variants
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.main()
