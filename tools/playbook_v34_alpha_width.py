#!/usr/bin/env python3
"""Test a real signed-short quotient carrier in V34 updateAlpha."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'updateAlpha')
    assert fn.count('\t\tint r;') == 1
    narrow = fn.replace('\t\tint r;', '\t\tshort r;', 1)
    return {'baseline': source, 'short-quotient': source[:start] + narrow + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '900311af'
    driver.OUT_NAME = 'playbook-v34-alpha-width'
    driver.SOURCE_PATHS = ('src/pump/v34/V34TX.c',)
    driver.variants = variants
    driver.main()
