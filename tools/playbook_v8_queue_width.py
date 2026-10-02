#!/usr/bin/env python3
"""Cross the independently observed signed-word V8 queue counters."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    cells = {'baseline': source}
    for label, names in (
        ('rx-short', ('v8_rxreadqueue',)),
        ('tx-short', ('v8_txwritequeue',)),
        ('both-short', ('v8_rxreadqueue', 'v8_txwritequeue')),
    ):
        text = source
        for name in names:
            start, end, fn = driver.function(text, name)
            assert fn.count('\tint i;') == 1
            fn = fn.replace('\tint i;', '\tshort i;')
            text = text[:start] + fn + text[end:]
        cells[label] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '379d400b'
    driver.OUT_NAME = 'playbook-v8-queue-width'
    driver.SOURCE_PATHS = ('src/v8/V8global.c',)
    driver.variants = variants
    driver.main()
