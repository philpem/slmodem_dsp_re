#!/usr/bin/env python3
"""Nine complete-TU controls for evidenced MRF input and ring boundaries."""
from itertools import product
import playbook_small_patterns as driver
from playbook_mrf_state_width import variants as widths


def variants(path, source):
    narrowed = widths(path, source)['short-state-produced']
    cells = {'baseline': source}
    for needed, shortfall, conditional in product((False, True), repeat=3):
        text = narrowed
        if needed:
            old = '''\t\tfor (k = 0; k < need; k++) {
\t\t\twidx = advance(widx, hlen);
\t\t\thistory[widx] = *in++;
\t\t}'''
            new = '''\t\t{
\t\t\tshort ntodo = need;

\t\t\twhile (ntodo-- != 0) {
\t\t\t\twidx = advance(widx, hlen);
\t\t\t\thistory[widx] = *in++;
\t\t\t}
\t\t}'''
            assert text.count(old) == 1
            text = text.replace(old, new)
        if shortfall:
            old = '\t\t\twhile (remaining-- > 0) {'
            assert text.count(old) == 1
            text = text.replace(old, '\t\t\twhile (remaining-- != 0) {')
        if conditional:
            old = '\treturn (next < len) ? next : 0;'
            assert text.count(old) == 1
            text = text.replace(old, '\tif (next >= len)\n\t\tnext = 0;\n\treturn next;')
        label = '-'.join(n for n, yes in zip(('needed-countdown', 'shortfall-nonzero', 'in-place-wrap'), (needed, shortfall, conditional)) if yes) or 'narrowed-control'
        cells[label] = text
    assert len(cells) == len(set(cells.values())) == 9
    return cells


if __name__ == '__main__':
    driver.REV = 'cbd16911'
    driver.OUT_NAME = 'playbook-mrf-loop-boundaries'
    driver.DUMP_FLAGS = ()
    driver.SOURCE_PATHS = ('src/dsp/fpm_mrf.c',)
    driver.variants = variants
    driver.main()
