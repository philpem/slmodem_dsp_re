#!/usr/bin/env python3
"""Eight full-TU scale/output-lifetime/post-decrement generator controls."""
import playbook_small_patterns as driver
from playbook_fpm_tone_demod import variants as demod_variants


def variants(path, source):
    parents = demod_variants(path, source)
    cells = {}
    for label, parent in parents.items():
        cells[label] = parent
        start, end, fn = driver.function(parent, 'FPM_TONE_generate_demod')
        assert fn.count('\tint i;') == 1
        fn = fn.replace('\tint i;', '\tshort i;')
        old = 'for (i = (short)(count - 1); i != -1; i = (short)(i - 1))'
        assert fn.count(old) == 1
        fn = fn.replace(old, 'for (i = count; i-- != 0;)')
        cells[label + '-post-decrement'] = parent[:start] + fn + parent[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    driver.REV = '9cb6d002'
    driver.OUT_NAME = 'playbook-fpm-tone-demod-cross'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    driver.main()
