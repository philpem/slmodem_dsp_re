#!/usr/bin/env python3
"""Separate cached AGC carrier widths from witnessed predicate-use widths."""
import playbook_small_patterns as d


def variants(path,source):
    cells={}
    for gain in ('int','local','use'):
        for level in ('int','local','use'):
            text=source
            if gain=='local':
                assert text.count('int mult = agc->mult;')==1
                text=text.replace('int mult = agc->mult;','short mult = agc->mult;')
            if gain=='use':
                assert text.count('mult == 0')==1 and text.count('mult != 0')==1
                text=text.replace('mult == 0','(short)mult == 0').replace('mult != 0','(short)mult != 0')
            if level=='local':
                assert text.count('int level_est = agc->level;')==1
                text=text.replace('int level_est = agc->level;','short level_est = agc->level;')
            if level=='use':
                assert text.count('level_est != 0')==1
                text=text.replace('level_est != 0','(short)level_est != 0')
            cells['baseline' if gain==level=='int' else gain+'-'+level]=text
    assert len(set(cells.values()))==9
    return cells


if __name__=='__main__':
    d.REV='6eaeaedd'
    d.SOURCE_PATHS=('src/dsp/fpm_agc.c',)
    d.OUT_NAME='gcc3-agc-predicate'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
