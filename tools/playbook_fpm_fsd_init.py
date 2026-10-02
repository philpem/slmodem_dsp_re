#!/usr/bin/env python3
"""Full-TU short clearing-counter and unsigned trace-allocation cross."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FPM_FSD_init')
    assert body.count('\tint i;') == 1
    assert body.count('(unsigned)state->cfg.trace_len') == 1
    cells = {}
    for short, unsigned in product((False, True), repeat=2):
        text = body
        if short: text = text.replace('\tint i;', '\tshort i;')
        if unsigned: text = text.replace('(unsigned)state->cfg.trace_len', '(unsigned short)state->cfg.trace_len')
        label = 'baseline' if not (short or unsigned) else '-'.join(n for n, yes in zip(('short-counter','unsigned-trace'),(short,unsigned)) if yes)
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '2f58aed6'
    driver.OUT_NAME = 'playbook-fpm-fsd-init'
    driver.SOURCE_PATHS = ('src/dsp/fpm_fsd.c',)
    driver.variants = variants
    driver.main()
