#!/usr/bin/env python3
"""Two full-TU controls for FPM_TONE_create's unsupported allocation guard."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'FPM_TONE_create')
    old = '\t\tif (state == NULL)\n\t\t\treturn NULL;\n'
    assert fn.count(old) == 1
    return {'baseline': source, 'unchecked-allocation':
            source[:start] + fn.replace(old, '') + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'a50cc9cf'
    driver.OUT_NAME = 'playbook-tone-alloc-guard'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    driver.main()
