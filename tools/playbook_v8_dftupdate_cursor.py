#!/usr/bin/env python3
"""Test the observed advancing DFT sample cursor, retaining inner loads."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'v8_dftupdate')
    old = '\tfor (j = 0; j < nsamples; j++) {'
    assert fn.count(old) == fn.count('int x = samples[j];') == 1
    changed = fn.replace(old, '\tfor (j = 0; j < nsamples; j++, samples++) {')
    changed = changed.replace('int x = samples[j];', 'int x = *samples;')
    return {'baseline': source, 'sample-cursor':
            source[:start] + changed + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '379d400b'
    driver.OUT_NAME = 'playbook-v8-dftupdate-cursor'
    driver.SOURCE_PATHS = ('src/v8/V8Dftc.c',)
    driver.variants = variants
    driver.main()
