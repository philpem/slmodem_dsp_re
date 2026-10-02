#!/usr/bin/env python3
"""Four full-TU multi-tone detector loop and energy-width controls."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FPM_MTD_detect')
    cells = {}
    for countdown, words in product((False, True), repeat=2):
        text = body
        if countdown:
            assert text.count('\tint i;') == text.count('samples[i]') == 1
            text = text.replace('\tint i;', '\tshort i;')
            text = text.replace('for (i = 0; i < count; i++)', 'for (i = (short)(count - 1); i != -1; i--)')
            text = text.replace('samples[i]', '*samples++')
        if words:
            for name in ('wideband', 'tone', 'out_of_band'):
                old = '\tint ' + name + (' =' if name == 'wideband' else ';')
                assert text.count(old) == 1
                text = text.replace(old, old.replace('int ', 'short ', 1), 1)
        label = 'baseline' if not (countdown or words) else '-'.join(n for n, yes in (('countdown',countdown),('words',words)) if yes)
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '43ef6841'
    driver.OUT_NAME = 'playbook-mtd-detect'
    driver.SOURCE_PATHS = ('src/dsp/fpm_mtd.c',)
    driver.variants = variants
    driver.main()
