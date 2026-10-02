#!/usr/bin/env python3
"""Discriminate signed mode branches from the blob's three unsigned tests."""
import sys
import playbook_small_patterns as driver
from playbook_fixedrc_factory import variants as factory


def variants(path, source):
    candidate = factory(path, source)['factory-contract']
    old = 'RcFixed_Create(int mode)'
    assert candidate.count(old) == 1
    return {'baseline': source, 'factory-signed': candidate,
            'factory-unsigned': candidate.replace(old, 'RcFixed_Create(unsigned int mode)')}


def overlay(path, label):
    if label != 'factory-unsigned':
        return {}
    text = (driver.ROOT/'include/dsplib/fixedrc.h').read_text()
    old = 'struct rc *RcFixed_Create(int mode);'
    assert text.count(old) == 1
    return {'dsplib/fixedrc.h': text.replace(old,
            'struct rc *RcFixed_Create(unsigned int mode);')}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '53c3bd00'
    driver.OUT_NAME = 'playbook-fixedrc-mode-type'
    driver.SOURCE_PATHS = ('src/core/FixedRC.c',)
    driver.variants = variants
    driver.HEADER_OVERLAYS = overlay
    driver.main()
