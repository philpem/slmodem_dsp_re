#!/usr/bin/env python3
"""Cross two observable transmit-queue store-order and direct-wrap boundaries."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'txwritequeue')
    old = '\t\t((short *)p)[1] = 0;\n\t\t((short *)p)[0] = src[i];'
    wrap = '\t\tp = q_next(q, p, V34_TXQ_END);'
    assert fn.count(old) == fn.count(wrap) == 1
    cells = {}
    for order in (False, True):
        for direct in (False, True):
            body = fn
            if order:
                body = body.replace(old,
                    '\t\t((short *)p)[0] = src[i];\n\t\t((short *)p)[1] = 0;')
            if direct:
                body = body.replace(wrap,
                    '\t\tif ((char *)p >= (char *)q + V34_TXQ_END)\n\t\t\tp = q->ring;')
            label = ('baseline' if not (order or direct) else
                     'order-%s_wrap-%s' % (
                         'low-first' if order else 'high-first',
                         'direct' if direct else 'helper'))
            cells[label] = source[:start] + body + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'fcf0427a'
    driver.OUT_NAME = 'playbook-v34-txqueue'
    driver.SOURCE_PATHS = ('src/pump/v34/V34TX.c',)
    driver.variants = variants
    driver.main()
