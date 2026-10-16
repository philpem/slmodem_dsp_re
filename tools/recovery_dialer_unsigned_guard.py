#!/usr/bin/env python3
"""Replay the witnessed unsigned DialerAbort range boundary on PR291."""
import playbook_small_patterns as driver
from gcc3_batch20_dialer_abort_reproduce import variants

if __name__ == '__main__':
    driver.REV = 'd127aec8'
    driver.OUT_NAME = 'recovery-dialer-unsigned-guard'
    driver.SOURCE_PATHS = ('src/dialer/Dialer.c',)
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    driver.variants = variants
    driver.main()
