#!/usr/bin/env python3
"""Two complete-TU controls for notch's evidenced x87 addition tree."""
import playbook_small_patterns as driver


def variants(path, source):
    old = 'state[0] = coef[1] * w + coef[0] * in + state[1];'
    new = 'state[0] = coef[0] * in + (coef[1] * w + state[1]);'
    assert source.count(old) == 1
    return {'baseline': source, 'feedback-first': source.replace(old, new)}


if __name__ == '__main__':
    driver.REV = '2be7a9e8'
    driver.OUT_NAME = 'playbook-notch-grouping'
    driver.SOURCE_PATHS = ('src/dsp/Notch.c',)
    driver.variants = variants
    driver.main()
