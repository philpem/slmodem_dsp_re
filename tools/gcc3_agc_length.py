#!/usr/bin/env python3
"""Separate full chosen AGC block length from the word sample countdown."""
import playbook_small_patterns as d
from gcc3_agc_partition import variants as previous


def variants(path,source):
    fixed=previous(path,source)['small-tail-first']
    old='unsigned short len = (unsigned short)(i ? block_len : tail);'
    assert fixed.count(old)==1
    return {'baseline':fixed,'full-length':fixed.replace(old,'unsigned len = i ? block_len : tail;')}


if __name__=='__main__':
    d.REV='6eaeaedd'
    d.SOURCE_PATHS=('src/dsp/fpm_agc.c',)
    d.OUT_NAME='gcc3-agc-length'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
