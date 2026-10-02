#!/usr/bin/env python3
"""Cross observed V8 shaping-filter counter width and coefficient traversal."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start = source.index('\nv8_fsktxfilter(') + 1
    end = source.index('\n}', start) + 2
    fn = source[start:end]
    cells = {'baseline': source}
    for label, narrow, cursor in (
        ('short-counter', True, False),
        ('tap-cursor', False, True),
        ('both', True, True),
    ):
        changed = fn
        assert changed.count('\tint i;') == 1
        if narrow:
            changed = changed.replace('\tint i;', '\tshort i;')
        if cursor:
            changed = changed.replace('\tint acc = 0x8000;',
                '\tconst short *coeff = v->v21_taps;\n\tint acc = 0x8000;')
            assert changed.count('v->v21_taps[i]') == 1
            changed = changed.replace('v->v21_taps[i]', '*coeff++')
        cells[label] = source[:start] + changed + source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'afedb41d'
    driver.OUT_NAME = 'playbook-v8-fsktx-controls'
    driver.SOURCE_PATHS = ('src/v8/V8Dpsk.c',)
    driver.variants = variants
    driver.main()
