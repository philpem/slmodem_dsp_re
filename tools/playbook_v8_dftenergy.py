#!/usr/bin/env python3
"""Test the object's advancing typed DFT-bin cursor without changing arithmetic."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'v8_dftenergy')
    old = '\tfor (i = 0; i < n; i++) {'
    assert fn.count(old) == 1
    changed = fn.replace(old, '\tfor (i = 0; i < n; i++, bin++) {')
    for field in ('re', 'im', 'energy'):
        assert changed.count('bin[i].' + field) == 1
        changed = changed.replace('bin[i].' + field, 'bin->' + field)
    return {'baseline': source, 'bin-cursor':
            source[:start] + changed + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '20821d7c'
    driver.OUT_NAME = 'playbook-v8-dftenergy'
    driver.SOURCE_PATHS = ('src/v8/V8Dftc.c',)
    driver.variants = variants
    driver.main()
