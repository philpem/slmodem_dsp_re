#!/usr/bin/env python3
"""Test tone-queue output traversal without changing loop or call types."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'v8_TONEq_generate')
    assert fn.count('out[i] = v8_cosread') == 1
    changed = fn.replace('out[i] = v8_cosread', '*out++ = v8_cosread')
    return {'baseline': source, 'output-cursor':
            source[:start] + changed + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'e70c56b9'
    driver.OUT_NAME = 'playbook-v8-tonequeue-cursor'
    driver.SOURCE_PATHS = ('src/v8/V8.c',)
    driver.variants = variants
    driver.main()
