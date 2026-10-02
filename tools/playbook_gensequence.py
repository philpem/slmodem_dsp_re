#!/usr/bin/env python3
"""Four full-TU GenSequence countdown/narrowing source controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'GenSequence')
    assert fn.count('\tunsigned short i;') == 1
    assert fn.count('\tfor (i = 0; i < count; i++) {') == 1
    assert fn.count('(index * width)') == 1
    old = '\t\tindex = (unsigned short)((index - 1) &\n\t\t\t\t\t hdx->gen_index_mask);'
    assert fn.count(old) == 1
    cells = {}
    for label, countdown, post in [('baseline', False, False),
                                    ('countdown', True, False),
                                    ('index-postdecrement', False, True),
                                    ('both', True, True)]:
        changed = fn
        if countdown:
            changed = changed.replace('\tunsigned short i;\n', '')
            changed = changed.replace('\tfor (i = 0; i < count; i++) {', '\twhile (count-- != 0) {')
        if post:
            changed = changed.replace('(index * width)', '(index-- * width)')
            changed = changed.replace(old, '\t\tindex &= hdx->gen_index_mask;')
        cells[label] = source[:start] + changed + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = 'e98ea74b'
    driver.OUT_NAME = 'playbook-gensequence'
    driver.SOURCE_PATHS = ('src/pump/v32/V32prc.c',)
    driver.variants = variants
    driver.main()
