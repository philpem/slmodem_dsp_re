#!/usr/bin/env python3
"""Finite counter, output-walk and read-width controls for V34 receive queue."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'rxreadqueue')
    cells = {}
    for short in (False, True):
        for walk in (False, True):
            for word in (False, True):
                label = ('baseline' if not (short or walk or word) else
                         'counter-%s_output-%s_read-%s' % (
                             'short' if short else 'int',
                             'walk' if walk else 'index',
                             'half' if word else 'whole'))
                body = fn
                assert body.count('\tint i;') == 1
                assert body.count('out[i] = (short)*p;') == 1
                if short:
                    body = body.replace('\tint i;', '\tshort i;')
                value = '*(const short *)p' if word else '(short)*p'
                store = '*out++' if walk else 'out[i]'
                body = body.replace('out[i] = (short)*p;', store + ' = ' + value + ';')
                cells[label] = source[:start] + body + source[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '9565d51c'
    driver.OUT_NAME = 'playbook-v34-rxqueue'
    driver.SOURCE_PATHS = ('src/pump/v34/V34RX.c',)
    driver.variants = variants
    driver.main()
