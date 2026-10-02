#!/usr/bin/env python3
"""Full-TU cross of four object-observed detector construction boundaries."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, "FPM_MTD_create")
    cells = {}
    fields = '''\t\tstate->cfg.coeff = cfg->coeff;
\t\tstate->cfg.tones = cfg->tones;
\t\tstate->cfg.ratio = cfg->ratio;
\t\tstate->cfg.min_level = cfg->min_level;
\t\tstate->cfg.f0a = cfg->f0a;'''
    allocation = '''\tif (owned)
\t\tstate->acc = sysdep_malloc(
\t\t\t(unsigned)state->cfg.tones * 2 * sizeof(short));'''
    guard = '\t\tif (state == NULL)\n\t\t\treturn NULL;\n'
    for copy, counter, elements, unchecked in product((False, True), repeat=4):
        text = body
        if copy:
            assert text.count(fields) == 1
            text = text.replace(fields, '\t\tstate->cfg = *cfg;')
        if counter:
            assert text.count('\tint i;') == 1
            text = text.replace('\tint i;', '\tshort i;')
        if elements:
            assert text.count(allocation) == 1
            text = text.replace(allocation, '''\tif (owned) {
\t\tshort elements = state->cfg.tones * 2;
\n\t\tstate->acc = sysdep_malloc(elements * sizeof(short));
\t}''')
        if unchecked:
            assert text.count(guard) == 1
            text = text.replace(guard, '')
        label = 'baseline' if not any((copy, counter, elements, unchecked)) else '-'.join(n for n, yes in zip(('aggregate', 'short-counter', 'short-elements', 'unchecked'), (copy, counter, elements, unchecked)) if yes)
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 16
    return cells


if __name__ == '__main__':
    driver.REV = '4188d010'
    driver.OUT_NAME = 'playbook-fpm-mtd-create'
    driver.SOURCE_PATHS = ('src/dsp/fpm_mtd.c',)
    driver.variants = variants
    driver.main()
