#!/usr/bin/env python3
"""Two complete-TU controls for the observed signed-word MRF output counter."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FPM_MRF_filter')
    old = '\tint produced = 0;'
    assert body.count(old) == 1
    candidate = body.replace(old, '\tshort produced = 0;')
    cells = {'baseline': source,
             'short-produced': source[:start] + candidate + source[end:]}
    assert len(set(cells.values())) == 2
    return cells


if __name__ == '__main__':
    driver.REV = 'cbd16911'
    driver.OUT_NAME = 'playbook-mrf-counter'
    driver.DUMP_FLAGS = ()
    driver.SOURCE_PATHS = ('src/dsp/fpm_mrf.c',)
    driver.variants = variants
    driver.main()
