#!/usr/bin/env python3
"""Four full-TU absolute encoder countdown/cursor source controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'SMCv32_encoder_abs')
    loop = 'for (i = 0; i < count; i++)'
    load = '(unsigned short)in[i]'
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
            changed = changed.replace(load, '(unsigned short)*in++')
        if countdown and cursor:
            changed = changed.replace('\tunsigned int i;\n', '')
            changed = changed.replace('for (i = 0; count-- != 0; i++)', 'while (count-- != 0)')
        cells[label] = source[:start] + changed + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '5ec53f63'
    driver.OUT_NAME = 'playbook-v32-smc-abs'
    driver.SOURCE_PATHS = ('src/pump/v32/V32SMC_TX.c',)
    driver.variants = variants
    driver.main()
