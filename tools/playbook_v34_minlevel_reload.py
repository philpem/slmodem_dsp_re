#!/usr/bin/env python3
"""Test the observed post-threshold-store configuration read for diagnostics."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'VPcmV34SetMinimumSigLevel')
    old = '"minSigLevel set to %d\\n", level, thresh);'
    assert fn.count(old) == 1
    changed = fn.replace(old, '"minSigLevel set to %d\\n",\n'
                         '\t\t\t\t     *(const int *)(cfg + CFG_MIN_LEVEL), thresh);')
    return {'baseline': source, 'debug-reload': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'b75131e2'
    driver.OUT_NAME = 'playbook-v34-minlevel-reload'
    driver.SOURCE_PATHS = ('src/pump/v34/VPcmV34Main.cpp',)
    driver.variants = variants
    driver.main()
