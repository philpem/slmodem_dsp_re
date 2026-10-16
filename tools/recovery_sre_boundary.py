#!/usr/bin/env python3
"""Cross caller count promotion with the SRE formal on complete period TUs."""
import subprocess

import playbook_small_patterns as driver

REV = '4169cb3b3108a840a51bca9ebfa086102c4c69c6'
HEADER = 'include/dsplib/fpm_sre.h'
CALLEE = 'src/dsp/fpm_sre.c'
CALLERS = ('src/fax/V17r_int.c', 'src/fax/V27r_int.c', 'src/fax/V29r_int.c')
CONTROL = 'src/pump/v32/V32int.c'


def variants(path, source):
    if path == CALLEE:
        old = 'short *out, short count)'
        assert source.count(old) == 1
        assert source.count('short remaining = count;') == 1
        return {'baseline': source,
                'int-formal': source.replace(old, 'short *out, int count)')
                .replace('short remaining = count;', 'short remaining = (short)count;')}
    if path == CONTROL:
        return {'baseline': source, 'int-formal': source}
    assert path in CALLERS
    assert source.count('(short)n);') == 1
    unsigned = source.replace('(short)n);', 'n);')
    # Two unchanged source strings intentionally exercise different headers.
    return {'baseline': source, 'short-unsigned': unsigned,
            'int-signed': source, 'int-unsigned': unsigned}


def overlays(path, label):
    if not label.startswith('int-'):
        return {}
    header = subprocess.check_output(
        ['git', 'show', REV + ':' + HEADER], cwd=driver.ROOT, text=True)
    old = 'short *out, short count);'
    assert header.count(old) == 1
    return {'dsplib/fpm_sre.h': header.replace(old, 'short *out, int count);')}


if __name__ == '__main__':
    driver.REV = REV
    driver.OUT_NAME = 'recovery-sre-boundary'
    driver.SOURCE_PATHS = (CALLEE,) + CALLERS + (CONTROL,)
    driver.variants = variants
    driver.HEADER_OVERLAYS = overlays
    driver.DUMP_FLAGS = ()
    driver.main()
