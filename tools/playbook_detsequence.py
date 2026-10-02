#!/usr/bin/env python3
"""Four complete-TU DetSequence outer-count/inner-shift loop controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'DetSequence')
    assert fn.count('\tunsigned short i;') == 1
    assert fn.count('\tfor (i = 0; i < count; i++) {') == 1
    assert fn.count('(word >> (nbits - 1 - bit))') == 1
    cells = {}
    for name, countdown, shift_counter in [('baseline', False, False),
                                           ('countdown', True, False),
                                           ('shift-counter', False, True),
                                           ('both', True, True)]:
        body = fn
        if countdown:
            body = body.replace('\tunsigned short i;\n', '')
            body = body.replace('\tfor (i = 0; i < count; i++) {',
                                '\twhile (count-- != 0) {')
        if shift_counter:
            body = body.replace('\t\tshort bit;','\t\tshort bit;\n\t\tshort shift = (short)(nbits - 1);')
            body = body.replace('(word >> (nbits - 1 - bit))', '(word >> shift--)')
        cells[name] = source[:start] + body + source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = 'e98ea74b'
    driver.OUT_NAME = 'playbook-detsequence'
    driver.SOURCE_PATHS = ('src/pump/v32/V32prc.c',)
    driver.variants = variants
    driver.main()
    from detsequence_analysis import analyze
    analyze(driver.ROOT, driver.OUT_NAME)
