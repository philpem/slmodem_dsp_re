#!/usr/bin/env python3
"""Eight full-TU scalar IIR count/cursor/feed-forward boundary controls."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FPM_iir_filt')
    cells = {}
    for count, cursors, narrow in product((False, True), repeat=3):
        text = body
        if count:
            assert text.count('\tint i;') == 1
            text = text.replace('\tint i;', '\tshort i;').replace('for (i = 0; i < sections; i++)', 'for (i = sections; i-- != 0;)')
        if narrow:
            old = '\t\tff += (coeff[3] * w2) >> 14;'
            assert text.count(old) == 1
            text = text.replace(old, old + '\n\t\tff = (short)ff;')
        if cursors:
            text = text.replace('state[0] = (short)w2;', '*state++ = (short)w2;')
            text = text.replace('state[1] = (short)w;', '*state++ = (short)w;')
            old = '\t\tff += (coeff[3] * w2) >> 14;'
            assert text.count(old) == 1
            text = text.replace(old, old + '\n\t\tcoeff += 4;')
            text = text.replace('coeff[4] * w', '*coeff++ * w')
            tail = '\n\t\tcoeff += FPM_IIR_COEFF_PER_SECTION;\n\t\tstate += FPM_IIR_STATE_PER_SECTION;'
            assert text.count(tail) == 1
            text = text.replace(tail, '')
        label = 'baseline' if not (count or cursors or narrow) else '-'.join(n for n, yes in (('count',count),('cursors',cursors),('narrow',narrow)) if yes)
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    driver.REV = '43ef6841'
    driver.OUT_NAME = 'playbook-iir-boundaries'
    driver.SOURCE_PATHS = ('src/dsp/fpm_iir.c',)
    driver.variants = variants
    driver.main()
