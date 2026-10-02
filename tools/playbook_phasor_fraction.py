#!/usr/bin/env python3
"""Two complete-TU interpolation-fraction conversion-boundary controls."""
import playbook_small_patterns as driver


def variants(path, source):
    old = '\tint frac = phase - (idx << 5);'
    assert source.count(old) == 1
    return {'baseline': source, 'short-fraction': source.replace(old, '\tshort frac = phase - (idx << 5);')}


if __name__ == '__main__':
    driver.REV = 'b3d741f7'
    driver.OUT_NAME = 'playbook-phasor-fraction'
    driver.SOURCE_PATHS = ('src/dsp/fpm_phasor.c',)
    driver.variants = variants
    driver.main()
