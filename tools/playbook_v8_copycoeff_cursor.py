#!/usr/bin/env python3
"""Test the observed advancing short-array coefficient copy pointers."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'v8_copycoeff')
    assert fn.count('\t\tdst[i] = src[i];') == 1
    changed = fn.replace('\t\tdst[i] = src[i];', '\t\t*dst++ = *src++;')
    return {'baseline': source, 'copy-cursors':
            source[:start] + changed + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'afedb41d'
    driver.OUT_NAME = 'playbook-v8-copycoeff-cursor'
    driver.SOURCE_PATHS = ('src/v8/V8global.c',)
    driver.variants = variants
    driver.main()
