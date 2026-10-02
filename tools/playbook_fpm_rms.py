#!/usr/bin/env python3
"""Four full-TU FPM_rms countdown/cursor source controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'FPM_rms')
    loop = 'for (i = 0; i < count; i++)'
    load = 'int x = samples[i];'
    assert fn.count(loop) == fn.count(load) == 1
    cells = {}
    for label, countdown, cursor in [('baseline', False, False),
                                     ('countdown', True, False),
                                     ('cursor', False, True),
                                     ('both', True, True)]:
        changed = fn
        if countdown:
            changed = changed.replace(loop, 'for (i = 0; count-- != 0; i++)')
        if cursor:
            changed = changed.replace(load, 'int x = *samples++;')
        if countdown and cursor:
            changed = changed.replace('\tunsigned i;\n', '')
            changed = changed.replace('for (i = 0; count-- != 0; i++)', 'while (count-- != 0)')
        cells[label] = source[:start] + changed + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '62e9866f'
    driver.OUT_NAME = 'playbook-fpm-rms'
    driver.SOURCE_PATHS = ('src/dsp/fpm_rms.c',)
    driver.variants = variants
    driver.main()
