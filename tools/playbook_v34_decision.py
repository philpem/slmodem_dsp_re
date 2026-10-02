#!/usr/bin/env python3
"""Cross independently observed best-distance width and advancing point cursor."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'decision')
    assert fn.count('int best_dist = 0x7fff;') == 1
    assert fn.count('\tshort i;') == 1
    old = '\tfor (i = 0; i < npts; i++) {\n\t\tconst int *p = &pts[i];'
    assert fn.count(old) == 1
    cells = {}
    for word in (False, True):
        for walk in (False, True):
            body = fn
            if word:
                body = body.replace('int best_dist = 0x7fff;', 'short best_dist = 0x7fff;')
            if walk:
                body = body.replace('\tshort i;', '\tshort i;\n\tconst int *p = pts;')
                body = body.replace(old, '\tfor (i = 0; i < npts; i++, p++) {')
            label = ('baseline' if not (word or walk) else
                     'distance-%s_points-%s' % (
                         'short' if word else 'int', 'walk' if walk else 'index'))
            cells[label] = source[:start] + body + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '361ef919'
    driver.OUT_NAME = 'playbook-v34-decision'
    driver.SOURCE_PATHS = ('src/pump/v34/V34RX.c',)
    driver.variants = variants
    driver.main()
