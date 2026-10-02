#!/usr/bin/env python3
"""Four complete-TU original-word consumption and fraction-width controls."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    old = '\tint frac = phase - (idx << 5);'
    assert source.count(old) == 1
    cells = {}
    for unsigned, narrow in product((False, True), repeat=2):
        label = 'baseline' if not (unsigned or narrow) else '-'.join(n for n, yes in zip(('unsigned-word','short-fraction'),(unsigned,narrow)) if yes)
        line = '\t' + ('short' if narrow else 'int') + ' frac = ' + ('(unsigned short)phase' if unsigned else 'phase') + ' - (idx << 5);'
        cells[label] = source.replace(old, line)
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '04d3177a'
    driver.OUT_NAME = 'playbook-phasor-consumption'
    driver.SOURCE_PATHS = ('src/dsp/fpm_phasor.c',)
    driver.variants = variants
    driver.main()
