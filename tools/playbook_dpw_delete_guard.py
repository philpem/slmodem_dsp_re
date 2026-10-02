#!/usr/bin/env python3
"""Two complete-TU controls for the blob's nonnull wrapper destructor."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'dp_wrapper_delete')
    old = '\tif (w == NULL)\n\t\treturn;\n\n'
    assert fn.count(old) == 1
    recovered = source[:start] + fn.replace(old, '') + source[end:]
    return {'baseline': source, 'nonnull-contract': recovered}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '0cb87c39'
    driver.OUT_NAME = 'playbook-dpw-delete-guard'
    driver.SOURCE_PATHS = ('src/core/dp_wrapper.c',)
    driver.variants = variants
    driver.main()
