#!/usr/bin/env python3
"""Cross observed initial count capture and old-value countdown tests."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'MakeTxData')
    old = '\t\tfor (i = *count; i != 0; i = (short)(i - 1))'
    assert fn.count(old) == 4
    cells = {'baseline': source}
    for label, capture, post in (
        ('captured-count', True, False),
        ('postdecrement', False, True),
        ('both', True, True),
    ):
        changed = fn
        if post:
            changed = changed.replace(old, '\t\ti = *count;\n\t\twhile (i-- != 0)')
        if capture:
            changed = changed.replace('\tshort i;', '\tshort i = *count;', 1)
            changed = changed.replace('for (i = *count; i != 0;', 'for (; i != 0;')
            changed = changed.replace('\t\ti = *count;\n', '')
        cells[label] = source[:start] + changed + source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'd672e822'
    driver.OUT_NAME = 'playbook-v22-txdata-controls'
    driver.SOURCE_PATHS = ('src/pump/v22/v22prc.c',)
    driver.variants = variants
    driver.main()
