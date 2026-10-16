#!/usr/bin/env python3
"""Restore writable storage for the original V.22 and B.103 AGC locals."""
import playbook_small_patterns as driver


def variants(path, source):
    changed = source
    for name in ('AGC_DEF_ALPHA', 'AGC_DEF_BETA'):
        old = 'static const short ' + name + '[2]'
        assert changed.count(old) == 1
        changed = changed.replace(old, 'static short ' + name + '[2]')
    return {'baseline': source, 'writable-original': changed}


if __name__ == '__main__':
    driver.REV = '4d2efa2d'
    driver.SOURCE_PATHS = ('src/pump/v22/v22rxtab.c', 'src/pump/b103/B103tab.c')
    driver.OUT_NAME = 'recovery-agc-writable-storage'
    driver.DUMP_FLAGS = ('-v', '-save-temps')
    driver.variants = variants
    driver.main()
