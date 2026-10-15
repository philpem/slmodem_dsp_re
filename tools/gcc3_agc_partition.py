#!/usr/bin/env python3
"""Test original small-tail fallthrough on the recovered AGC guard."""
import playbook_small_patterns as d
from gcc3_agc_guard import variants as previous


def variants(path,source):
    fixed=previous(path,source)['both']
    old='''	if ((unsigned short)(block_len >> 1) <= (unsigned short)tail)
		blocks = (unsigned short)(blocks + 1);
	else
		tail = (unsigned short)(tail + block_len);'''
    new='''	if ((unsigned short)(block_len >> 1) > (unsigned short)tail)
		tail = (unsigned short)(tail + block_len);
	else
		blocks = (unsigned short)(blocks + 1);'''
    assert fixed.count(old)==1
    return {'baseline':fixed,'small-tail-first':fixed.replace(old,new)}


if __name__=='__main__':
    d.REV='6eaeaedd'
    d.SOURCE_PATHS=('src/dsp/fpm_agc.c',)
    d.OUT_NAME='gcc3-agc-partition'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
