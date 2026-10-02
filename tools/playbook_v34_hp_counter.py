#!/usr/bin/env python3
"""Test the independently observed signed-word high-pass loop counter."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'V34TimingHPFilter')
    assert fn.count('\tint k;') == 1
    changed = fn.replace('\tint k;', '\tshort k;')
    return {'baseline': source, 'short-counter':
            source[:start] + changed + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '065e39c0'
    driver.OUT_NAME = 'playbook-v34-hp-counter'
    driver.SOURCE_PATHS = ('src/pump/v34/v34filters.c',)
    driver.variants = variants
    driver.main()
