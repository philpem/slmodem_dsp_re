#!/usr/bin/env python3
"""Four full-TU Float2Linear countdown/cursor controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'zFLTUTL_Float2Linear')
    loop = 'for (i = 0; i < n; i++)'
    store = 'dst[i] = (short)(src[i] * gain);'
    assert fn.count(loop) == fn.count(store) == 1
    cells = {}
    for label, countdown, cursor in [('baseline', False, False),
                                     ('countdown', True, False),
                                     ('cursor', False, True),
                                     ('both', True, True)]:
        changed = fn
        if countdown:
            changed = changed.replace(loop, 'for (i = 0; n > 0; --n, ++i)')
        if cursor:
            changed = changed.replace(store, '*dst++ = (short)(*src++ * gain);')
        if countdown and cursor:
            changed = changed.replace('\tint i;\n\n', '')
            changed = changed.replace('for (i = 0; n > 0; --n, ++i)', 'for (; n > 0; --n)')
        cells[label] = source[:start] + changed + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = 'd7e118ca'
    driver.OUT_NAME = 'playbook-float2linear'
    driver.SOURCE_PATHS = ('src/service/Beepgen.c',)
    driver.variants = variants
    driver.main()
